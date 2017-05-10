// DCIMG_LabVIEW.cpp: definisce le funzioni esportate per l'applicazione DLL.
//

#include "stdafx.h"


void pause(int dur)
{
	Sleep(dur * 1000);
}

void writeTiffTags(libtiff::TIFF* output, libtiff::uint32 XSIZE, libtiff::uint32 YSIZE, libtiff::uint16 bpp, libtiff::uint16 spp, int page, int lb)
{
	TIFFSetField(output, TIFFTAG_IMAGEWIDTH, XSIZE);
	TIFFSetField(output, TIFFTAG_IMAGELENGTH, YSIZE);
	TIFFSetField(output, TIFFTAG_BITSPERSAMPLE, bpp);
	TIFFSetField(output, TIFFTAG_SAMPLESPERPIXEL, spp);	// this value is fixed to 1. We are using grayscale images
	TIFFSetField(output, TIFFTAG_ROWSPERSTRIP, YSIZE);
	TIFFSetField(output, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
	TIFFSetField(output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
	/* We are writing single page of the multipage file */
	TIFFSetField(output, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
	TIFFSetField(output, TIFFTAG_PAGENUMBER, (libtiff::uint16)page, (libtiff::uint16)lb);
}

void downsampleImage(libtiff::uint16 *img16, int height, int width)	// performs 2x downsampling
{
	libtiff::uint16 A, B, C, D;

	for (int i = 0; i<height / 2; i++)	// smart approach! an in-place conversion!
	{
		for (int j = 0; j<width / 2; j++)
		{
			//computing 8-neighbours
			A = img16[2 * i*width + 2 * j];
			B = img16[2 * i*width + (2 * j + 1)];
			C = img16[(2 * i + 1)*width + 2 * j];
			D = img16[(2 * i + 1)*width + (2 * j + 1)];
			//computing mean
			img16[i*(width / 2) + j] = (libtiff::uint16) floor(((double)A + (double)B + (double)C + (double)D) / (double)4 + 0.5); // now it's overflow-safe
		}
	}
}

//typedef DCIMG_ERR (DCIMGAPI *dcimg_init_t) ( DCIMG_INIT* param );
typedef DCIMG_ERR(DCIMGAPI *dcimg_openA_t) (DCIMG_OPENA* param);	// seem to be pointers to function inside the library. Still have to completely understand the syntax
typedef DCIMG_ERR(DCIMGAPI *dcimg_lockframe_t) (HDCIMG hdcimg, DCIMG_FRAME* aFrame);
typedef DCIMG_ERR(DCIMGAPI *dcimg_getparaml_t) (HDCIMG hdcimg, DCIMG_IDPARAML index, int32* paraml);
typedef DCIMG_ERR(DCIMGAPI *dcimg_close_t) (HDCIMG hdcimg);

__declspec(dllexport) int convertDCIMG(char *Source, char *Dest, char *smallDest, int L_MAX, int downsample_int, int zStep, int zcoord, int lim, int zdownsample)
{
	/* variables declarations */

	HINSTANCE hinstLib2;  // pointer to the library
	libtiff::TIFF *output;
	libtiff::TIFF *smallout;
	int32 totframe, width, height, pixeltype, width2, height2;	// DCIMG file parameters. Here we are assuming they are int32 (which is true for this version of dcimgapi, but it's not guaranteed for the next)
	libtiff::uint16 spp = 1, bpp;
	libtiff::uint32 XSIZE, YSIZE;
	int nb, lb, nlb;
	bool deleteThis = true;	// other boolean flag to decide dcimg files deletion. It is set at runtime to FALSE if the converter finds any issue.
	string zName, zName2;
	string img_name;
	int zcurrent;
	string completeImgDst(Dest);
	string smallImgDst(smallDest);
	bool downsample = downsample_int == 1;

	/* Opening dcimgapi.dll and importing functions */

	//dcimg_init_t dcimg_init = NULL;
	dcimg_openA_t dcimg_openA = NULL;
	dcimg_lockframe_t dcimg_lockframe = NULL;
	dcimg_getparaml_t dcimg_getparaml = NULL;
	dcimg_close_t dcimg_close = NULL;

	hinstLib2 = LoadLibrary(TEXT("dcimgapi.dll"));  // loads dcimgapi.dll library

	if (NULL == hinstLib2)
		return 1;

	//dcimg_init = (dcimg_init_t) GetProcAddress(hinstLib2, "dcimg_init");
	dcimg_openA = (dcimg_openA_t)GetProcAddress(hinstLib2, "dcimg_openA");		// retrieves the library functions that will be used
	dcimg_lockframe = (dcimg_lockframe_t)GetProcAddress(hinstLib2, "dcimg_lockframe");
	dcimg_getparaml = (dcimg_getparaml_t)GetProcAddress(hinstLib2, "dcimg_getparaml");
	dcimg_close = (dcimg_close_t)GetProcAddress(hinstLib2, "dcimg_close");

	if (dcimg_openA == NULL || dcimg_lockframe == NULL || dcimg_getparaml == NULL || dcimg_close == NULL)
		return 2;

	DCIMG_ERR errVal = (DCIMG_ERR)0xffffffff;

	/* open dcimg file */

	DCIMG_OPENA *param = new DCIMG_OPENA; // creates the structure for opening parameters and sets the right size and path
	param->size = (int32) sizeof(DCIMG_OPENA);
	param->path = Source;
	errVal = dcimg_openA(param);	// open the DCIMG files and check for errors. Here you can see the problem of having already deleted the txt file!
	if (errVal == DCIMG_ERR_FILENOTOPENED)
		return 3;

	// here below: retrieve dcimg files parameters (number of frames, frame width, frame height, pixel type -- this latter can be either 8 or 16 bit).
	errVal = dcimg_getparaml(param->hdcimg, DCIMG_IDPARAML_NUMBEROF_TOTALFRAME, &totframe);
	errVal = dcimg_getparaml(param->hdcimg, DCIMG_IDPARAML_IMAGE_WIDTH, &width);
	errVal = dcimg_getparaml(param->hdcimg, DCIMG_IDPARAML_IMAGE_HEIGHT, &height);
	errVal = dcimg_getparaml(param->hdcimg, DCIMG_IDPARAML_IMAGE_PIXELTYPE, &pixeltype);

	// here below: translating dcimg parameters into tiff parameters.
	XSIZE = (libtiff::uint32)width; YSIZE = (libtiff::uint32)height;
	bpp = (libtiff::uint16)pixeltype * 8; // also this is valid only with the current dcimg_api library.

	nb = (int)ceil((double)totframe / (double)L_MAX);	// number of 3D tiff files to be generated
	lb = totframe / nb;		// the converter will generate (nb-nlb) files with lb pages, and nlb files with lb+1 pages. 
	nlb = totframe % nb;	// the sum turns out to be exactly totframe

	libtiff::TIFFSetWarningHandler(0);

	//unsigned char* data = new unsigned char[spp*pixeltype*width*height];
	DCIMG_FRAME *frame = new DCIMG_FRAME;	// creates the structure for DCIMG frame
	frame->size = (int32) sizeof(DCIMG_FRAME);
	frame->iKind = (int32)0;	// the header files says "reserved. Set to 0". does it mean we have to actively write them?????
	frame->option = (int32)0;	// same as above
	frame->type = (DCIMG_PIXELTYPE)0;	// same as above
	frame->iFrame = (int32)0;
	string completeImgPath, smallImgPath;
	int j, k;
	int pageCount = 0;
	zcurrent = zcoord;

	/* copy data (in case with downsampling) into the 3D tiff files */

	for (j = 0; j<nb; j++){	// loop over tiff files -- whose number is nb

		if (j < nb - nlb){ // for the first nb-nlb files, the number of page is lb, for the last nlb it is (lb+1)
			k = lb;
		}
		else{
			k = lb + 1;
		}

		completeImgPath = completeImgDst;	// creates file name
		zName = (to_string(zcurrent));//zstep in um, zcoord in decimi di um
		if (zName.length()>6)
			return 4;

		zName.insert(0, 6 - zName.length(), '0');	// zero padding
		completeImgPath.append(zName);

		char *fullPath = new char[completeImgPath.length() + 4 + 1];
		strcpy(fullPath, completeImgPath.c_str());
		strcat(fullPath, ".tif");
		output = libtiff::TIFFOpen(fullPath, "w");	// open tiff file

		for (int page = 0; page < k; page++){	// loop over pages of a single tiff file
			errVal = dcimg_lockframe(param->hdcimg, frame);	// try to read the dcimg frame
			if (errVal != 1)
				return 5;

			libtiff::uint16 *img16 = (libtiff::uint16 *) frame->buf; // new libtiff::uint16[width*height];
			img16 = (libtiff::uint16*) frame->buf;

			if (!downsample){
				// sets TIFF tags. This should be done for each and every page
				writeTiffTags(output, XSIZE, YSIZE, bpp, spp, page, k);
				// write the page (we previously set the strip as big as the page) onto the tiff file
				TIFFWriteEncodedStrip(output, 0, (unsigned char*)frame->buf, spp*pixeltype*width*height);
			}
			else{
				downsampleImage(img16, height, width);
				writeTiffTags(output, XSIZE / 2, YSIZE / 2, bpp, spp, page, k);
				TIFFWriteEncodedStrip(output, 0, (unsigned char*)frame->buf, spp*pixeltype*width / 2 * height / 2);
			}

			if (pageCount%zdownsample == 0){	// every zdownsample pages, save a single-page tiff with very low res
				smallImgPath = smallImgDst;	// creates file name
				zName2 = (to_string(zcoord + pageCount * 10 * zStep));//zstep in um, zcoord in decimi di um
				if (zName2.length() > 6)
					return 4;

				zName2.insert(0, 6 - zName2.length(), '0');	// zero padding
				smallImgPath.append(zName2);
				char *smallPath = new char[smallImgPath.length() + 4 + 1];
				strcpy(smallPath, smallImgPath.c_str());
				strcat(smallPath, ".tif");
				smallout = libtiff::TIFFOpen(smallPath, "w");

				if (downsample){
					height2 = height / 2;
					width2 = width / 2;
				}
				else{
					height2 = height;
					width2 = width;
				}

				for (int powers = 0; powers < lim; powers++){
					downsampleImage(img16, height2, width2);
					width2 = width2 / 2;
					height2 = height2 / 2;
				}

				writeTiffTags(smallout, width2, height2, bpp, spp, 0, 1);
				TIFFWriteEncodedStrip(smallout, 0, (unsigned char*)frame->buf, spp*pixeltype*width2*height2);
				TIFFClose(smallout);
			}

			pageCount++;		// increase page counter
			frame->iFrame++;	// increase frame index

			if (!TIFFWriteDirectory(output))
				return 6;
		}

		TIFFClose(output);
		zcurrent = zcurrent + k*zStep * 10;

	}

	/* close the dcimg file and delete files when appropriate */

	errVal = dcimg_close(param->hdcimg);	// closes the dcimg file and delets param and frame structures
	delete param;
	delete frame;
	if (errVal != 1)
		return 8;

	FreeLibrary(hinstLib2);

	return 0;

}


