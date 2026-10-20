/*
 * Fliqlo for RenkuOS - a flip clock screen saver for Haiku.
 *
 * Two dark cards on black, hours and minutes, each split across the middle;
 * when a value changes its top half flips down to reveal the new one. In
 * 12-hour mode the hour card carries AM/PM in its top left corner.
 *
 * Written so that gcc2 builds it: on an x86_gcc2 hybrid the screen_blanker
 * only loads add-ons of the primary architecture. No C++11.
 */

#include <Bitmap.h>
#include <CheckBox.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <ScreenSaver.h>
#include <Slider.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <stdio.h>
#include <string.h>
#include <time.h>


static const bigtime_t kFlipDuration = 420000;
static const bigtime_t kTick = 1000000 / 30;

static const rgb_color kBlack = { 0, 0, 0, 255 };
static const rgb_color kCard = { 22, 22, 22, 255 };
static const rgb_color kDigit = { 192, 192, 192, 255 };

enum {
	MSG_24_HOUR = 'f24h',
	MSG_SIZE = 'fsiz',
	MSG_BRIGHTNESS = 'fbri'
};


static rgb_color
dim(rgb_color c, float f)
{
	rgb_color r;
	r.red = (uint8)(c.red * f);
	r.green = (uint8)(c.green * f);
	r.blue = (uint8)(c.blue * f);
	r.alpha = 255;
	return r;
}


class Fliqlo;


class ConfigView : public BView {
public:
							ConfigView(Fliqlo* saver);
	virtual	void			AttachedToWindow();
	virtual	void			MessageReceived(BMessage* message);

private:
			Fliqlo*			fSaver;
			BCheckBox*		f24Hour;
			BSlider*		fSize;
			BSlider*		fBrightness;
};


// One card: hours or minutes. Holds the face it shows and, while flipping,
// the face it is leaving.
struct Card {
	int				value;
	int				previous;
	bigtime_t		flipStart;
	BBitmap*		face;
	BBitmap*		oldFace;
	BRect			frame;
};


class Fliqlo : public BScreenSaver {
public:
							Fliqlo(BMessage* archive, image_id id);
	virtual					~Fliqlo();

	virtual	status_t		StartSaver(BView* view, bool preview);
	virtual	void			StopSaver();
	virtual	void			Draw(BView* view, int32 frame);
	virtual	void			StartConfig(BView* view);
	virtual	void			StopConfig();
	virtual	status_t		SaveState(BMessage* into) const;

			bool			fTwentyFour;
			int32			fSizePercent;
			int32			fBrightnessPercent;
			// Bumped by the config view: the next Draw() lays out again.
			volatile int32	fGeneration;

private:
			void			_Layout(BRect bounds);
			void			_FreeBitmaps();
			BBitmap*		_RenderFace(int value, bool isHour);
			void			_DrawCard(BView* target, Card& card, bigtime_t now);
			void			_CurrentValues(int& hour, int& minute, bool& pm);

			BBitmap*		fBuffer;
			BView*			fBufferView;
			Card			fCards[2];
			BRect			fBounds;
			float			fSide;
			BFont			fDigitFont;
			BFont			fLabelFont;
			bool			fPM;
			bool			fDirty;
			int32			fLaidOutGeneration;
};


// #pragma mark - Fliqlo


Fliqlo::Fliqlo(BMessage* archive, image_id id)
	:
	BScreenSaver(archive, id),
	fTwentyFour(false),
	fSizePercent(100),
	fBrightnessPercent(100),
	fGeneration(0),
	fBuffer(NULL),
	fBufferView(NULL),
	fSide(0),
	fPM(false),
	fDirty(true),
	fLaidOutGeneration(-1)
{
	if (archive != NULL) {
		bool b;
		int32 i;
		if (archive->FindBool("24hour", &b) == B_OK)
			fTwentyFour = b;
		if (archive->FindInt32("size", &i) == B_OK && i >= 50 && i <= 100)
			fSizePercent = i;
		if (archive->FindInt32("brightness", &i) == B_OK && i >= 10 && i <= 100)
			fBrightnessPercent = i;
	}
	for (int i = 0; i < 2; i++) {
		fCards[i].value = -1;
		fCards[i].previous = -1;
		fCards[i].flipStart = 0;
		fCards[i].face = NULL;
		fCards[i].oldFace = NULL;
	}
	SetTickSize(kTick);
}


Fliqlo::~Fliqlo()
{
	_FreeBitmaps();
}


status_t
Fliqlo::SaveState(BMessage* into) const
{
	into->AddBool("24hour", fTwentyFour);
	into->AddInt32("size", fSizePercent);
	into->AddInt32("brightness", fBrightnessPercent);
	return B_OK;
}


status_t
Fliqlo::StartSaver(BView* view, bool preview)
{
	fLaidOutGeneration = -1;
	return B_OK;
}


void
Fliqlo::StopSaver()
{
}


void
Fliqlo::_FreeBitmaps()
{
	delete fBuffer;
	fBuffer = NULL;
	fBufferView = NULL;
	for (int i = 0; i < 2; i++) {
		delete fCards[i].face;
		delete fCards[i].oldFace;
		fCards[i].face = NULL;
		fCards[i].oldFace = NULL;
		fCards[i].value = -1;
		fCards[i].previous = -1;
		fCards[i].flipStart = 0;
	}
}


static void
pick_digit_font(BFont& font)
{
	// Fliqlo's digits are Helvetica Condensed Bold. The package installs
	// TeX Gyre Heros Condensed Bold, a free Helvetica clone (GUST Font
	// License); without it, the boldest sans there is.
	static const char* kFaces[][2] = {
		{ "TeX Gyre Heros Cn", "Bold" },
		{ "Noto Sans Display", "Bold" },
		{ "Noto Sans", "Bold" },
		{ NULL, NULL }
	};
	for (int i = 0; kFaces[i][0] != NULL; i++) {
		if (font.SetFamilyAndStyle(kFaces[i][0], kFaces[i][1]) == B_OK)
			return;
	}
	font = *be_bold_font;
}


static BRect
glyph_box(const BFont& font, const char* text)
{
	const char* strings[1] = { text };
	BRect boxes[1];
	escapement_delta delta = { 0, 0 };
	font.GetBoundingBoxesForStrings(strings, 1, B_SCREEN_METRIC, &delta,
		boxes);
	return boxes[0];
}


void
Fliqlo::_Layout(BRect bounds)
{
	_FreeBitmaps();
	fBounds = bounds;
	fLaidOutGeneration = fGeneration;

	float width = bounds.Width() + 1;
	float height = bounds.Height() + 1;
	bool portrait = height > width;
	float scale = fSizePercent / 100.0f;

	float side;
	if (portrait)
		side = (height * 0.43f < width * 0.82f ? height * 0.43f : width * 0.82f);
	else
		side = (width * 0.43f < height * 0.82f ? width * 0.43f : height * 0.82f);
	side = (float)(int)(side * scale);
	if (side < 8)
		side = 8;
	fSide = side;
	float gap = (float)(int)(side * 0.08f + 1);

	BRect card(0, 0, side - 1, side - 1);
	if (portrait) {
		float top = bounds.top + (height - 2 * side - gap) / 2;
		float left = bounds.left + (width - side) / 2;
		fCards[0].frame = card.OffsetToCopy((int)left, (int)top);
		fCards[1].frame = card.OffsetToCopy((int)left, (int)(top + side + gap));
	} else {
		float left = bounds.left + (width - 2 * side - gap) / 2;
		float top = bounds.top + (height - side) / 2;
		fCards[0].frame = card.OffsetToCopy((int)left, (int)top);
		fCards[1].frame = card.OffsetToCopy((int)(left + side + gap), (int)top);
	}

	// Digits two thirds of the card tall, as long as "88" still fits
	// across it. Measured at 100 points and scaled.
	pick_digit_font(fDigitFont);
	fDigitFont.SetSize(100);
	BRect digits = glyph_box(fDigitFont, "88");
	float size = 100 * side * 0.66f / (digits.Height() > 1 ? digits.Height() : 70);
	if (digits.Width() * size / 100 > side * 0.9f)
		size = 100 * side * 0.9f / digits.Width();
	fDigitFont.SetSize(size);
	fDigitFont.SetFlags(B_FORCE_ANTIALIASING);

	// AM/PM capitals about a fifteenth of the card tall.
	fLabelFont = fDigitFont;
	fLabelFont.SetSize(100);
	BRect caps = glyph_box(fLabelFont, "PM");
	fLabelFont.SetSize(100 * side * 0.068f / (caps.Height() > 1 ? caps.Height() : 70));

	fBuffer = new BBitmap(bounds.OffsetToCopy(B_ORIGIN), B_RGB32, true);
	fBufferView = new BView(fBuffer->Bounds(), "buffer", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fBuffer->AddChild(fBufferView);
	fDirty = true;
}


BBitmap*
Fliqlo::_RenderFace(int value, bool isHour)
{
	float bright = fBrightnessPercent / 100.0f;
	BRect r(0, 0, fSide - 1, fSide - 1);
	BBitmap* bitmap = new BBitmap(r, B_RGB32, true);
	BView* view = new BView(r, "face", B_FOLLOW_NONE, B_WILL_DRAW);
	bitmap->AddChild(view);
	bitmap->Lock();

	view->SetHighColor(kBlack);
	view->FillRect(r);
	float radius = fSide * 0.075f;
	view->SetHighColor(dim(kCard, bright));
	view->FillRoundRect(r, radius, radius);

	char text[8];
	if (isHour && !fTwentyFour)
		snprintf(text, sizeof(text), "%d", value);
	else
		snprintf(text, sizeof(text), "%02d", value);

	// Centre the glyphs themselves, not the line box: a digit sits on the
	// baseline and has no descender.
	view->SetFont(&fDigitFont);
	BRect box = glyph_box(fDigitFont, text);
	float x = (fSide - box.Width()) / 2 - box.left;
	float y = (fSide + box.Height()) / 2 - box.bottom;
	if (!box.IsValid() || box.Height() < 1) {
		x = (fSide - view->StringWidth(text)) / 2;
		y = fSide / 2 + fDigitFont.Size() * 0.36f;
	}
	view->SetHighColor(dim(kDigit, bright));
	view->SetLowColor(dim(kCard, bright));
	view->SetDrawingMode(B_OP_OVER);
	view->DrawString(text, BPoint(x, y));

	if (isHour && !fTwentyFour) {
		// Bottom left, as on Fliqlo.
		view->SetFont(&fLabelFont);
		view->DrawString(fPM ? "PM" : "AM",
			BPoint(fSide * 0.07f, fSide * 0.875f));
	}

	// The split: a black line across the middle of the card.
	float line = fSide * 0.006f;
	if (line < 1)
		line = 1;
	view->SetHighColor(kBlack);
	view->FillRect(BRect(0, fSide / 2 - line / 2, fSide - 1,
		fSide / 2 + line / 2 - 1 + (line < 2 ? 1 : 0)));

	view->Sync();
	bitmap->Unlock();

	// Everything on a card is grey. Text follows the system's subpixel
	// antialiasing, which leaves colour fringes on digits this large; taking
	// every pixel to its grey level removes them.
	uint8* row = (uint8*)bitmap->Bits();
	int32 bpr = bitmap->BytesPerRow();
	int32 rows = bitmap->Bounds().IntegerHeight() + 1;
	int32 columns = bitmap->Bounds().IntegerWidth() + 1;
	for (int32 j = 0; j < rows; j++, row += bpr) {
		uint8* p = row;
		for (int32 i = 0; i < columns; i++, p += 4) {
			uint8 g = (uint8)((p[0] + p[1] + p[2]) / 3);
			p[0] = p[1] = p[2] = g;
		}
	}
	return bitmap;
}


void
Fliqlo::_CurrentValues(int& hour, int& minute, bool& pm)
{
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	hour = local.tm_hour;
	minute = local.tm_min;
	pm = hour >= 12;
	if (!fTwentyFour) {
		hour %= 12;
		if (hour == 0)
			hour = 12;
	}
}


static float
ease_in(float t)
{
	return t * t;
}


static float
ease_out(float t)
{
	return 1 - (1 - t) * (1 - t);
}


void
Fliqlo::_DrawCard(BView* target, Card& card, bigtime_t now)
{
	BRect frame = card.frame;
	float half = fSide / 2;
	BRect topSrc(0, 0, fSide - 1, half - 1);
	BRect bottomSrc(0, half, fSide - 1, fSide - 1);
	BRect topDst(frame.left, frame.top, frame.right, frame.top + half - 1);
	BRect bottomDst(frame.left, frame.top + half, frame.right, frame.bottom);

	bigtime_t elapsed = now - card.flipStart;
	if (card.oldFace == NULL || elapsed >= kFlipDuration) {
		target->DrawBitmap(card.face, frame);
		return;
	}

	float t = (float)elapsed / kFlipDuration;
	// Behind the flap: the new top half above, the old bottom half below.
	target->DrawBitmap(card.face, topSrc, topDst);
	target->DrawBitmap(card.oldFace, bottomSrc, bottomDst);

	target->SetDrawingMode(B_OP_ALPHA);
	target->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
	if (t < 0.5f) {
		// The old top half folds down onto the hinge.
		float k = 1 - ease_in(t * 2);
		float h = (half - 1) * k;
		if (h >= 1) {
			BRect flap(frame.left, frame.top + half - 1 - h, frame.right,
				frame.top + half - 1);
			target->SetDrawingMode(B_OP_COPY);
			target->DrawBitmap(card.oldFace, topSrc, flap);
			target->SetDrawingMode(B_OP_ALPHA);
			target->SetHighColor(0, 0, 0, (uint8)(150 * (1 - k)));
			target->FillRect(flap);
		}
		// The new top half is in the shadow of the flap.
		target->SetHighColor(0, 0, 0, (uint8)(110 * k));
		target->FillRect(BRect(frame.left, frame.top, frame.right,
			frame.top + half - 1 - h));
	} else {
		// The new bottom half unfolds from the hinge.
		float k = ease_out((t - 0.5f) * 2);
		float h = (half - 1) * k;
		if (h >= 1) {
			BRect flap(frame.left, frame.top + half, frame.right,
				frame.top + half + h);
			target->SetDrawingMode(B_OP_COPY);
			target->DrawBitmap(card.face, bottomSrc, flap);
			target->SetDrawingMode(B_OP_ALPHA);
			target->SetHighColor(0, 0, 0, (uint8)(150 * (1 - k)));
			target->FillRect(flap);
		}
		// The old bottom half goes dark as the flap covers it.
		target->SetHighColor(0, 0, 0, (uint8)(110 * (1 - k)));
		target->FillRect(BRect(frame.left, frame.top + half + h, frame.right,
			frame.bottom));
	}
	target->SetDrawingMode(B_OP_COPY);
}


void
Fliqlo::Draw(BView* view, int32 frame)
{
	BRect bounds = view->Bounds();
	if (fBuffer == NULL || bounds != fBounds
		|| fLaidOutGeneration != fGeneration)
		_Layout(bounds);
	if (fBuffer == NULL || !fBuffer->IsValid())
		return;

	int hour, minute;
	bool pm;
	_CurrentValues(hour, minute, pm);
	bigtime_t now = system_time();
	int values[2] = { hour, minute };

	bool animating = false;
	for (int i = 0; i < 2; i++) {
		Card& card = fCards[i];
		if (card.value != values[i] || (i == 0 && pm != fPM)) {
			bool first = card.face == NULL;
			if (i == 0)
				fPM = pm;
			delete card.oldFace;
			card.oldFace = first ? NULL : card.face;
			card.previous = card.value;
			card.value = values[i];
			card.face = _RenderFace(card.value, i == 0);
			card.flipStart = now;
			fDirty = true;
		}
		if (card.oldFace != NULL) {
			if (now - card.flipStart < kFlipDuration)
				animating = true;
			else {
				delete card.oldFace;
				card.oldFace = NULL;
				fDirty = true;
			}
		}
	}

	if (!animating && !fDirty && frame != 0)
		return;

	fBuffer->Lock();
	fBufferView->SetHighColor(kBlack);
	fBufferView->FillRect(fBufferView->Bounds());
	for (int i = 0; i < 2; i++) {
		Card card = fCards[i];
		card.frame.OffsetBy(-bounds.left, -bounds.top);
		_DrawCard(fBufferView, card, now);
	}
	fBufferView->Sync();
	fBuffer->Unlock();

	view->DrawBitmap(fBuffer, bounds.LeftTop());
	fDirty = animating;
}


// #pragma mark - configuration


void
Fliqlo::StartConfig(BView* view)
{
	BLayoutBuilder::Group<>(view, B_VERTICAL)
		.Add(new ConfigView(this));
}


void
Fliqlo::StopConfig()
{
}


// Laid out with the layout API: the view the preferences hand to
// StartConfig() has not been sized yet, so frames computed from it come out
// empty.
ConfigView::ConfigView(Fliqlo* saver)
	:
	BView("fliqlo config", B_WILL_DRAW),
	fSaver(saver)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BStringView* title = new BStringView("title", "Fliqlo");
	title->SetFont(be_bold_font);
	BStringView* about = new BStringView("about", "A flip clock, for RenkuOS.");

	f24Hour = new BCheckBox("24hour", "24-hour clock",
		new BMessage(MSG_24_HOUR));
	f24Hour->SetValue(saver->fTwentyFour ? B_CONTROL_ON : B_CONTROL_OFF);

	fSize = new BSlider("size", "Size", new BMessage(MSG_SIZE), 50, 100,
		B_HORIZONTAL);
	fSize->SetLimitLabels("Small", "Large");
	fSize->SetValue(saver->fSizePercent);
	fSize->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fSize->SetHashMarkCount(6);

	fBrightness = new BSlider("brightness", "Brightness",
		new BMessage(MSG_BRIGHTNESS), 10, 100, B_HORIZONTAL);
	fBrightness->SetLimitLabels("Dim", "Bright");
	fBrightness->SetValue(saver->fBrightnessPercent);
	fBrightness->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fBrightness->SetHashMarkCount(10);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(title)
		.Add(about)
		.AddStrut(be_plain_font->Size())
		.Add(f24Hour)
		.Add(fSize)
		.Add(fBrightness)
		.AddGlue();
}


void
ConfigView::AttachedToWindow()
{
	f24Hour->SetTarget(this);
	fSize->SetTarget(this);
	fBrightness->SetTarget(this);
	fSize->SetModificationMessage(new BMessage(MSG_SIZE));
	fBrightness->SetModificationMessage(new BMessage(MSG_BRIGHTNESS));
}


void
ConfigView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_24_HOUR:
			fSaver->fTwentyFour = f24Hour->Value() == B_CONTROL_ON;
			break;
		case MSG_SIZE:
			fSaver->fSizePercent = fSize->Value();
			break;
		case MSG_BRIGHTNESS:
			fSaver->fBrightnessPercent = fBrightness->Value();
			break;
		default:
			BView::MessageReceived(message);
			return;
	}
	fSaver->fGeneration++;
}


// #pragma mark - entry point


extern "C" _EXPORT BScreenSaver*
instantiate_screen_saver(BMessage* message, image_id image)
{
	return new Fliqlo(message, image);
}
