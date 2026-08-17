#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const char *DEFAULT_CHARS =
    " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

void print_usage(const char *prog_name) {
  printf("Usage: %s [OPTIONS] <file_path>\n", prog_name);
  printf("Mode (Defaults to Image):\n");
  printf("  -v           Enable Video mode (requires ffmpeg)\n");
  printf("\nGeneral Options:\n");
  printf("  -w <width>   Set output width (defaults to terminal width)\n");
  printf("  -i           Invert colors (useful for dark backgrounds)\n");
  printf(
      "  -c <string>  Custom ASCII character palette (darkest to lightest)\n");
  printf("  -C           Enable 24-bit True Color RGB rendering\n");
  printf("\nImage Specific:\n");
  printf("  -b <char>    Background character for transparent pixels (default: "
         "space)\n");
  printf("\nVideo Specific:\n");
  printf("  -r <fps>     Set target playback framerate (default: 30)\n");
  printf("  -h           Show this help message\n");
}

int render_image(const char *path, int target_width, char *lut, int use_color,
                 char bg_char) {
  int orig_width, orig_height, channels;
  unsigned char *img =
      stbi_load(path, &orig_width, &orig_height, &channels, 4); // Force RGBA

  if (!img) {
    printf("Error: Could not load image %s\n", path);
    return 1;
  }

  int target_height = (orig_height * target_width) / (orig_width * 2);
  if (target_height == 0)
    target_height = 1;

  size_t bytes_per_pixel = use_color ? 25 : 1;
  size_t row_size = (target_width * bytes_per_pixel) + 6;
  size_t out_size = target_height * row_size;

  char *out_buffer = (char *)malloc(out_size + 1);
  if (!out_buffer) {
    printf("Error: Memory allocation failed\n");
    stbi_image_free(img);
    return 1;
  }

  char *ptr = out_buffer;

  for (int y = 0; y < target_height; y++) {
    for (int x = 0; x < target_width; x++) {
      int orig_x = (x * orig_width) / target_width;
      int orig_y = (y * orig_height) / target_height;
      int pixel_index = (orig_y * orig_width + orig_x) * 4;

      unsigned char r = img[pixel_index];
      unsigned char g = img[pixel_index + 1];
      unsigned char b = img[pixel_index + 2];
      unsigned char a = img[pixel_index + 3];

      if (a < 128) { // Transparency handling
        if (use_color)
          ptr += sprintf(ptr, "\x1b[0m%c", bg_char);
        else
          *ptr++ = bg_char;
        continue;
      }

      unsigned char gray = (r * 77 + g * 150 + b * 29) >> 8;
      char ascii = lut[gray];

      if (use_color)
        ptr += sprintf(ptr, "\x1b[38;2;%d;%d;%dm%c", r, g, b, ascii);
      else
        *ptr++ = ascii;
    }
    if (use_color)
      ptr += sprintf(ptr, "\x1b[0m\n");
    else
      *ptr++ = '\n';
  }
  *ptr = '\0';

  fputs(out_buffer, stdout);
  free(out_buffer);
  stbi_image_free(img);
  return 0;
}

int render_video(const char *path, int target_width, char *lut, int use_color,
                 int fps) {
  // Assume 16:9 video aspect ratio. Compensate for font height by dividing
  // by 2.
  int target_height = (target_width * 9) / (16 * 2);
  if (target_height == 0)
    target_height = 1;

  char command[512];
  snprintf(command, sizeof(command),
           "ffmpeg -loglevel quiet -i \"%s\" -vf "
           "\"scale=%d:%d,fps=%d\" "
           "-f rawvideo -pix_fmt rgb24 -",
           path, target_width, target_height, fps);

  FILE *pipe = popen(command, "r");
  if (!pipe) {
    printf("Error: Could not start ffmpeg process. Is it installed?\n");
    return 1;
  }

  size_t frame_bytes = target_width * target_height * 3;
  unsigned char *frame = (unsigned char *)malloc(frame_bytes);

  size_t bytes_per_pixel = use_color ? 25 : 1;
  size_t row_size = (target_width * bytes_per_pixel) + 6;
  size_t out_size = target_height * row_size;
  char *out_buffer = (char *)malloc(out_size + 1);

  printf("\x1b[2J"); // Clear screen once

  while (fread(frame, 1, frame_bytes, pipe) == frame_bytes) {
    char *ptr = out_buffer;

    for (int y = 0; y < target_height; y++) {
      for (int x = 0; x < target_width; x++) {
        int pixel_index = (y * target_width + x) * 3;
        unsigned char r = frame[pixel_index];
        unsigned char g = frame[pixel_index + 1];
        unsigned char b = frame[pixel_index + 2];

        unsigned char gray = (r * 77 + g * 150 + b * 29) >> 8;
        char ascii = lut[gray];

        if (use_color)
          ptr += sprintf(ptr, "\x1b[38;2;%d;%d;%dm%c", r, g, b, ascii);
        else
          *ptr++ = ascii;
      }
      if (use_color)
        ptr += sprintf(ptr, "\x1b[0m\n");
      else
        *ptr++ = '\n';
    }
    *ptr = '\0';

    // Reset cursor to top-left instead of clearing to prevent flicker
    printf("\x1b[H");
    fputs(out_buffer, stdout);
    fflush(stdout);

    usleep(1000000 / fps);
  }

  printf("\x1b[0m\n");
  pclose(pipe);
  free(frame);
  free(out_buffer);
  return 0;
}

int main(int argc, char **argv) {
  int target_width = 0;
  int target_fps = 30;
  int invert = 0;
  int use_color = 0;
  int is_video = 0;
  const char *ascii_chars = DEFAULT_CHARS;
  char bg_char = ' ';

  int opt;
  // Updated getopt string to include 'v' and combine all arguments cleanly
  while ((opt = getopt(argc, argv, "vw:r:ic:b:Ch")) != -1) {
    switch (opt) {
    case 'v':
      is_video = 1;
      break;
    case 'w':
      target_width = atoi(optarg);
      break;
    case 'r':
      target_fps = atoi(optarg);
      break;
    case 'i':
      invert = 1;
      break;
    case 'c':
      ascii_chars = optarg;
      break;
    case 'b':
      bg_char = optarg[0];
      break;
    case 'C':
      use_color = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    printf("Error: Missing file path.\n");
    print_usage(argv[0]);
    return 1;
  }

  const char *file_path = argv[optind];

  // Auto-detect terminal width
  if (target_width <= 0) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
      target_width = w.ws_col - 1;
    } else {
      target_width = 100;
    }
  }

  // Precompute LUT (Look-Up Table) for Grayscale -> ASCII mapping
  char lut[256];
  int num_chars = strlen(ascii_chars);
  for (int i = 0; i < 256; i++) {
    int index = (i * (num_chars - 1)) / 255;
    if (invert)
      index = (num_chars - 1) - index;
    lut[i] = ascii_chars[index];
  }

  // Route to the appropriate processing engine
  if (is_video) {
    return render_video(file_path, target_width, lut, use_color, target_fps);
  } else {
    return render_image(file_path, target_width, lut, use_color, bg_char);
  }
}
