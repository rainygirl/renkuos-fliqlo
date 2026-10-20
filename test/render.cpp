/*
 * Renders the Fliqlo add-on into PNG files without the screen_blanker, for
 * checking it over ssh:
 *
 *   render <add-on> <width> <height> <out-prefix> [24]
 *
 * Writes <prefix>-still.png, then waits for the next minute and writes four
 * frames of the flip.
 */

#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <Message.h>
#include <ScreenSaver.h>
#include <TranslatorRoster.h>
#include <View.h>

#include <image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


typedef BScreenSaver* (*instantiate_func)(BMessage*, image_id);


// PNG through the translators, or a PPM next to it where there is no PNG
// translator (minimum images have only the text one).
static void
save(BBitmap* bitmap, const char* path)
{
	BBitmap* copy = new BBitmap(bitmap);
	BBitmapStream stream(copy);
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &file,
			B_PNG_FORMAT) == B_OK) {
		printf("wrote %s\n", path);
		return;
	}
	char ppm[256];
	snprintf(ppm, sizeof(ppm), "%s.ppm", path);
	FILE* out = fopen(ppm, "wb");
	int32 w = bitmap->Bounds().IntegerWidth() + 1;
	int32 h = bitmap->Bounds().IntegerHeight() + 1;
	fprintf(out, "P6\n%d %d\n255\n", (int)w, (int)h);
	const uint8* row = (const uint8*)bitmap->Bits();
	for (int32 y = 0; y < h; y++, row += bitmap->BytesPerRow()) {
		for (int32 x = 0; x < w; x++) {
			fputc(row[x * 4 + 2], out);
			fputc(row[x * 4 + 1], out);
			fputc(row[x * 4], out);
		}
	}
	fclose(out);
	printf("wrote %s\n", ppm);
}


int
main(int argc, char** argv)
{
	if (argc < 5) {
		fprintf(stderr, "usage: render <add-on> <w> <h> <prefix> [24]\n");
		return 1;
	}
	BApplication app("application/x-vnd.rainygirl-fliqlo-render");
	image_id image = load_add_on(argv[1]);
	instantiate_func instantiate;
	if (image < 0 || get_image_symbol(image, "instantiate_screen_saver",
			B_SYMBOL_TYPE_TEXT, (void**)&instantiate) != B_OK) {
		fprintf(stderr, "cannot load %s\n", argv[1]);
		return 1;
	}

	BMessage settings;
	if (argc > 5)
		settings.AddBool("24hour", true);
	BRect r(0, 0, atoi(argv[2]) - 1, atoi(argv[3]) - 1);
	BBitmap bitmap(r, B_RGB32, true);
	BView* view = new BView(r, "screen", B_FOLLOW_NONE, B_WILL_DRAW);
	bitmap.AddChild(view);

	BScreenSaver* saver = instantiate(&settings, image);
	saver->StartSaver(view, false);
	char path[256];

	bitmap.Lock();
	saver->Draw(view, 0);
	view->Sync();
	bitmap.Unlock();
	snprintf(path, sizeof(path), "%s-still.png", argv[4]);
	save(&bitmap, path);

	// Up to the next minute, then the flip at a few points.
	time_t now = time(NULL);
	snooze((bigtime_t)(60 - now % 60) * 1000000 - 900000);
	int32 frame = 1;
	bigtime_t start = 0;
	int shot = 0;
	bigtime_t marks[4] = { 80000, 170000, 260000, 360000 };
	while (shot < 4) {
		bitmap.Lock();
		saver->Draw(view, frame++);
		view->Sync();
		bitmap.Unlock();
		if (start == 0 && time(NULL) % 60 == 0)
			start = system_time();
		if (start != 0 && system_time() - start >= marks[shot]) {
			snprintf(path, sizeof(path), "%s-flip%d.png", argv[4], shot);
			save(&bitmap, path);
			shot++;
		}
		snooze(5000);
	}
	saver->StopSaver();
	delete saver;
	return 0;
}
