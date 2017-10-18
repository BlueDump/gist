// jpeg_split.c: Write each scan from a multi-scan/progressive JPEG.
// This is based loosely on example.c from libjpeg, and should require only
// libjpeg as a dependency (e.g. gcc -ljpeg -o jpeg_split.o jpeg_split.c).
#include <stdio.h>
#include <jerror.h>
#include "jpeglib.h"
#include <setjmp.h>
#include <string.h>

void read_scan(struct jpeg_decompress_struct * cinfo,
               JSAMPARRAY buffer,
               char * base_output);
int read_JPEG_file (char * filename, int scanNumber, char * base_output);

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <Input JPEG> <Output base name>\n", argv[0]);
        printf("This reads in the progressive/multi-scan JPEG given and writes out each scan\n");
        printf("to a separate PPM file, named with the scan number.\n");
        return 1;
    }

    char * fname = argv[1];
    char * out_base = argv[2];
    read_JPEG_file(fname, 1, out_base);
    return 0;
}

struct error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

METHODDEF(void) error_exit (j_common_ptr cinfo) {
    struct error_mgr * err = (struct error_mgr *) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(err->setjmp_buffer, 1);
}

int read_JPEG_file (char * filename, int scanNumber, char * base_output) {
    struct jpeg_decompress_struct cinfo;
    struct error_mgr jerr;
    FILE * infile;      /* source file */
    JSAMPARRAY buffer;  /* Output row buffer */
    int row_stride;     /* physical row width in output buffer */

    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return 0;
    }

    // Set up the normal JPEG error routines, then override error_exit.
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = error_exit;
    // Establish the setjmp return context for error_exit to use:
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    (void) jpeg_read_header(&cinfo, TRUE);

    // Set some decompression parameters

    // Incremental reading requires this flag:
    cinfo.buffered_image = TRUE;
    // To perform fast scaling in the output, set these:
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;

    // Decompression begins...
    (void) jpeg_start_decompress(&cinfo);

    printf("JPEG is %s-scan\n", jpeg_has_multiple_scans(&cinfo) ? "multi" : "single");
    printf("Outputting %ix%i\n", cinfo.output_width, cinfo.output_height);

    // row_stride = JSAMPLEs per row in output buffer
    row_stride = cinfo.output_width * cinfo.output_components;
    // Make a one-row-high sample array that will go away when done with image
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // Start actually handling image data!
    while(!jpeg_input_complete(&cinfo)) {
        read_scan(&cinfo, buffer, base_output);
    }

    // Clean up.
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    if (jerr.pub.num_warnings) {
        printf("libjpeg indicates %i warnings\n", jerr.pub.num_warnings);
    }
}

void read_scan(struct jpeg_decompress_struct * cinfo,
               JSAMPARRAY buffer,
               char * base_output)
{
    char out_name[1024];
    FILE * outfile = NULL;
    int scan_num = 0;

    scan_num = cinfo->input_scan_number;
    jpeg_start_output(cinfo, scan_num);

    // Read up to the next scan.
    int status;
    do {
        status = jpeg_consume_input(cinfo);
    } while (status != JPEG_REACHED_SOS && status != JPEG_REACHED_EOI);

    // Construct a filename & write PPM image header.
    snprintf(out_name, 1024, "%s%i.ppm", base_output, scan_num);
    if ((outfile = fopen(out_name, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s for writing!\n", out_name);
        return;
    }
    fprintf(outfile, "P6\n%d %d\n255\n", cinfo->output_width, cinfo->output_height);

    // Read each scanline into 'buffer' and write it to the PPM.
    // (Note that libjpeg updates cinfo->output_scanline automatically)
    while (cinfo->output_scanline < cinfo->output_height) {
        jpeg_read_scanlines(cinfo, buffer, 1);
        fwrite(buffer[0], cinfo->output_components, cinfo->output_width, outfile);
    }

    jpeg_finish_output(cinfo);
    fclose(outfile);
}
