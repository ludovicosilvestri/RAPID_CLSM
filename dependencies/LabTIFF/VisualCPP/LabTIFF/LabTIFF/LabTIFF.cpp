// LabTIFF.cpp: library for basic multi-page TIFF I/O, suitable for embedding in LabVIEW.
//

#include "stdafx.h"

__declspec(dllexport) int OpenTIFFFile(char* path, char* OpeningPurpose, int*& FileRef)
{
	TIFF *tif = TIFFOpen(path, OpeningPurpose);
	if (tif!=0)
	{
		FileRef = (int*)tif;
		return 0;
	}
	else
		return 9000; 
}

__declspec(dllexport) int CloseTIFFFile(int*& FileRef)
{
	TIFF *tif = (TIFF*)FileRef;
	TIFFClose(tif);
	return 0;
}

__declspec(dllexport) int GetNumberOfPages(int*& FileRef, int& NumberOfPages)
{
	TIFF *tif = (TIFF*)FileRef;
	NumberOfPages = 0;
	do
	{
		NumberOfPages++;
	} while (TIFFReadDirectory(tif));

	return 0;
}

__declspec(dllexport) int GetFrameSize(int*& FileRef, uint32& ImageWidth, uint32& ImageHeight)
{
	int error;
	uint32 W, H;
	TIFF *tif = (TIFF*)FileRef;
	error = TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &W);
	error = TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &H);
	ImageWidth = W;
	ImageHeight = H;

	return error;
}

__declspec(dllexport) int AppendPage(int*& FileRef, double* DataArray, uint32 ImageHeight, uint32 ImageWidth, char* ImageType)
{
	uint16 SamplesPerPixel, BitsPerSample;
	uint8* DataArray_uint8 = (uint8*) DataArray;
	uint16* DataArray_uint16 = (uint16*)DataArray;
	int16* DataArray_int16 = (int16*)DataArray;
	uint32* DataArray_RGBA32 = (uint32*)DataArray;
	uint64* DataArray_RGBA64 = (uint64*)DataArray;
	float* DataArray_float32 = (float*)DataArray;
	tsize_t StripSize;
	uint32 rows;
	int nPixels = ImageWidth * ImageHeight;
	tstrip_t nStrips;


	if (!strcmp(ImageType, "uint8"))
	{
		BitsPerSample = 8;
		SamplesPerPixel = 1;
		DataArray_uint8 = new uint8[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_uint8[i] = (uint8)DataArray[i];
	}
	else if (!strcmp(ImageType, "uint16"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 1;
		DataArray_uint16 = new uint16[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_uint16[i] = (uint16)DataArray[i];
	}
	else if (!strcmp(ImageType, "int16"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 1;
		DataArray_int16 = new int16[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_int16[i] = (int16)DataArray[i];
	}
	else if (!strcmp(ImageType, "RGBA32"))
	{
		BitsPerSample = 8;
		SamplesPerPixel = 4;
		DataArray_RGBA32 = new uint32[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_RGBA32[i] = (uint32)DataArray[i];
	}
	else if (!strcmp(ImageType, "RGBA64"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 4;
		DataArray_RGBA64 = new uint64[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_RGBA64[i] = (uint64)DataArray[i];
	}
	else if (!strcmp(ImageType, "float32"))
	{
		BitsPerSample = 32;
		SamplesPerPixel = 1;
		DataArray_float32 = new float[nPixels];
		for (int i = 0; i < nPixels; i++)
			DataArray_float32[i] = (float)DataArray[i];
	}

	TIFF *tif = (TIFF*)FileRef;
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, ImageWidth);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, ImageHeight);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, SamplesPerPixel);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, BitsPerSample);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, 1); // black is zero
	TIFFSetField(tif, TIFFTAG_COMPRESSION, 5); // LZW compression

	rows = TIFFDefaultStripSize(tif, 0);
	TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rows);
	nStrips = TIFFNumberOfStrips(tif);
	StripSize = TIFFStripSize(tif);

	//  the code works fine for even number of rows. A proper handling of remainders should be introduced.
	// currently, the remainders are just not used.

	int striprows = (int)rows * (int)nStrips;
	if (striprows > (int)ImageHeight)
	{
		nStrips = nStrips - 1;
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, ImageHeight-1);
	}
	
	for (tstrip_t StripCount = 0; StripCount < nStrips; StripCount++)
	{
		if (!strcmp(ImageType, "uint8"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_uint8[StripCount * StripSize], StripSize);
		else if (!strcmp(ImageType, "uint16"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_uint16[(StripCount * StripSize)/2], StripSize);
		else if (!strcmp(ImageType, "int16"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_int16[(StripCount * StripSize)/2], StripSize);
		else if (!strcmp(ImageType, "RGBA32"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_RGBA32[(StripCount * StripSize)/4], StripSize);
		else if (!strcmp(ImageType, "RGBA64"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_RGBA64[(StripCount * StripSize)/4], StripSize);
		else if (!strcmp(ImageType, "float32"))
			TIFFWriteEncodedStrip(tif, StripCount, &DataArray_float32[(StripCount * StripSize)/4], StripSize);
	}
	
	TIFFWriteDirectory(tif);
	
	if (!strcmp(ImageType, "uint8"))
	{
		delete[] DataArray_uint8;
	}
	else if (!strcmp(ImageType, "uint16"))
	{
		delete[] DataArray_uint16;
	}
	else if (!strcmp(ImageType, "int16"))
	{
		delete[] DataArray_int16;
	}
	else if (!strcmp(ImageType, "RGBA32"))
	{
		delete[] DataArray_RGBA32;
	}
	else if (!strcmp(ImageType, "RGBA64"))
	{
		delete[] DataArray_RGBA64;
	}
	else if (!strcmp(ImageType, "float32"))
	{
		delete [] DataArray_float32;
	}

	return 0;
}

__declspec(dllexport) int GetPage(int*& FileRef, double* DataArray, uint32& ImageHeight, uint32& ImageWidth, char* ImageType, int frameindex)
{
	TIFF *tif = (TIFF*)FileRef;
	tdir_t dirnumber = (tdir_t)frameindex;
	uint16 SamplesPerPixel, BitsPerSample;
	uint8* DataArray_uint8 = (uint8*)DataArray;
	uint16* DataArray_uint16 = (uint16*)DataArray;
	int16* DataArray_int16 = (int16*)DataArray;
	uint32* DataArray_RGBA32 = (uint32*)DataArray;
	uint64* DataArray_RGBA64 = (uint64*)DataArray;
	float* DataArray_float32 = (float*)DataArray;
	tsize_t StripSize;
	uint32 rows,W,H;
	int error;

	if (!TIFFSetDirectory(tif, dirnumber))
	{
		return 9001;
	}
	
	error = TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &W);
	error = TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &H);
	ImageWidth = W;
	ImageHeight = H;

	int nPixels = W * H;
	tstrip_t nStrips;

	if (!strcmp(ImageType, "uint8"))
	{
		BitsPerSample = 8;
		SamplesPerPixel = 1;
		DataArray_uint8 = new uint8[nPixels];
	}
	else if (!strcmp(ImageType, "uint16"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 1;
		DataArray_uint16 = new uint16[nPixels];
	}
	else if (!strcmp(ImageType, "int16"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 1;
		DataArray_int16 = new int16[nPixels];
	}
	else if (!strcmp(ImageType, "RGBA32"))
	{
		BitsPerSample = 8;
		SamplesPerPixel = 4;
		DataArray_RGBA32 = new uint32[nPixels];
	}
	else if (!strcmp(ImageType, "RGBA64"))
	{
		BitsPerSample = 16;
		SamplesPerPixel = 4;
		DataArray_RGBA64 = new uint64[nPixels];
	}
	else if (!strcmp(ImageType, "float32"))
	{
		BitsPerSample = 32;
		SamplesPerPixel = 1;
		DataArray_float32 = new float[nPixels];
	}

	error = TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rows);
	nStrips = TIFFNumberOfStrips(tif);
	StripSize = TIFFStripSize(tif);

	//  the code works fine for even number of rows. A proper handling of remainders should be introduced.
	// currently, the remainders are just not used. CHECK THIS STUFF FOR READING PURPOSES

	int striprows = (int)rows * (int)nStrips;
	if (striprows > (int)H)
	{
		nStrips = nStrips - 1;
		//TIFFSetField(tif, TIFFTAG_IMAGELENGTH, ImageHeight - 1);
	}

	for (tstrip_t StripCount = 0; StripCount < nStrips; StripCount++)
	{
		if (!strcmp(ImageType, "uint8"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_uint8[StripCount * StripSize], StripSize);
		else if (!strcmp(ImageType, "uint16"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_uint16[(StripCount * StripSize) / 2], StripSize);
		else if (!strcmp(ImageType, "int16"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_int16[(StripCount * StripSize) / 2], StripSize);
		else if (!strcmp(ImageType, "RGBA32"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_RGBA32[(StripCount * StripSize) / 4], StripSize);
		else if (!strcmp(ImageType, "RGBA64"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_RGBA64[(StripCount * StripSize) / 4], StripSize);
		else if (!strcmp(ImageType, "float32"))
			error = TIFFReadEncodedStrip(tif, StripCount, &DataArray_float32[(StripCount * StripSize) / 4], StripSize);
	}

	if (!strcmp(ImageType, "uint8"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_uint8[i];
		//ImageHeight = (uint32)DataArray_uint8[51235];
		//ImageWidth = (uint32)DataArray[51235];
		delete[] DataArray_uint8;
	}
	else if (!strcmp(ImageType, "uint16"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_uint16[i];
		delete[] DataArray_uint16;
	}
	else if (!strcmp(ImageType, "int16"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_int16[i];
		delete[] DataArray_int16;
	}
	else if (!strcmp(ImageType, "RGBA32"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_RGBA32[i];
		delete[] DataArray_RGBA32;
	}
	else if (!strcmp(ImageType, "RGBA64"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_RGBA64[i];
		delete[] DataArray_RGBA64;
	}
	else if (!strcmp(ImageType, "float32"))
	{
		for (int i = 0; i < nPixels; i++)
			DataArray[i] = (double)DataArray_float32[i];
		delete[] DataArray_float32;
	}
	
	return error;
		
}