#include "itkImage.h"
#include "itkImportImageFilter.h"
#include "itkImageRegistrationMethodv4.h"
#include "itkTranslationTransform.h"
#include "itkMattesMutualInformationImageToImageMetricv4.h"
#include"itkOnePlusOneEvolutionaryOptimizerv4.h"
#include"itkNormalVariateGenerator.h" 
#include <iostream>

typedef double PixelType;

__declspec(dllexport) int ImageRegistrationMutualInfo(PixelType * Rob, PixelType * Roy, int width, int height, double start_x, double start_y, double percentage, unsigned int numberOfBins, int maxIteration, double * xTranslation, double * yTranslation, double * quality, double * numberOfIterations, char * error, int charlength)
{
	/* First part: takes care of importing the images from Rob and Roy buffers */

	typedef itk::Image <PixelType, 2> ImageType;
	typedef itk::ImportImageFilter < PixelType, 2> ImportFilterType;

	ImportFilterType::Pointer importFilterRob = ImportFilterType::New();
	ImportFilterType::Pointer importFilterRoy = ImportFilterType::New();
	ImportFilterType::SizeType size;
	size[0] = width;
	size[1] = height;

	ImportFilterType::IndexType start;
	start.Fill(0);

	ImportFilterType::RegionType region;
	region.SetIndex(start);
	region.SetSize(size);
	importFilterRob->SetRegion(region);
	importFilterRoy->SetRegion(region);
	
	const itk::SpacePrecisionType origin[2] = { 0.0,0.0 };
	importFilterRob->SetOrigin(origin);
	importFilterRoy->SetOrigin(origin);

	const itk::SpacePrecisionType spacing[2] = { 1.0,1.0 };
	importFilterRob->SetSpacing(spacing);
	importFilterRoy->SetSpacing(spacing);

	const unsigned int numberOfPixels = size[0] * size[1];
	const bool importImageFilterWillOwnTheBuffer = true;
	importFilterRob->SetImportPointer(Rob, numberOfPixels, importImageFilterWillOwnTheBuffer);
	importFilterRoy->SetImportPointer(Roy, numberOfPixels, importImageFilterWillOwnTheBuffer);

	/* Second part: image registration using mutual information */

	typedef itk::TranslationTransform<double, 2> TransformType;
	typedef itk::MattesMutualInformationImageToImageMetricv4<ImageType, ImageType> MetricType;
	typedef itk::OnePlusOneEvolutionaryOptimizerv4<double> OptimizerType;
	typedef itk::ImageRegistrationMethodv4<ImageType, ImageType, TransformType> RegistrationType;
	typedef itk::Statistics::NormalVariateGenerator GeneratorType;
	
	GeneratorType::Pointer generator = GeneratorType::New();
	generator->Initialize(12345);

	MetricType::Pointer metric = MetricType::New();
	OptimizerType::Pointer optimizer = OptimizerType::New();
	RegistrationType::Pointer registration = RegistrationType::New();

	TransformType::Pointer movingInitialTransform = TransformType::New();
	TransformType::ParametersType initialParameters(movingInitialTransform->GetNumberOfParameters());
	initialParameters[0] = start_x;
	initialParameters[1] = start_y;
	movingInitialTransform->SetParameters(initialParameters);

	//unsigned int numberOfBins = 50;
	metric->SetNumberOfHistogramBins(numberOfBins);
	metric->SetUseMovingImageGradientFilter(false);
	metric->SetUseFixedImageGradientFilter(false);

	double radius = 20;
	double epsilon = 0.1;

	if (registration->GetCurrentLevel() == 0) 
	{
		radius = 10;
		epsilon = 0.01;
	}
	else if (registration->GetCurrentLevel() == 1)
	{
		radius = 10;
		epsilon = 0.01;
	}
	
	
	optimizer->SetNormalVariateGenerator(generator);
	optimizer->Initialize(radius);
	optimizer->SetEpsilon(epsilon);
	optimizer->SetMaximumIteration(maxIteration);

	const unsigned int numberOfLevels = 3;
	RegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
	shrinkFactorsPerLevel.SetSize(numberOfLevels);
	shrinkFactorsPerLevel[0] = 4;
	shrinkFactorsPerLevel[1] = 2;
	shrinkFactorsPerLevel[2] = 1;

	RegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
	smoothingSigmasPerLevel.SetSize(numberOfLevels);
	smoothingSigmasPerLevel[0] = 0;
	smoothingSigmasPerLevel[1] = 0;
	smoothingSigmasPerLevel[2] = 0;

	registration->SetMetric(metric);
	
	RegistrationType::MetricSamplingStrategyType samplingStrategy = RegistrationType::RANDOM;
	registration->SetMetricSamplingStrategy(samplingStrategy);
	registration->SetMetricSamplingPercentage(percentage);

	registration->SetOptimizer(optimizer);
	registration->SetFixedImage(importFilterRob->GetOutput());
	registration->SetMovingImage(importFilterRoy->GetOutput());
	registration->SetMovingInitialTransform(movingInitialTransform);
	registration->SetNumberOfLevels(numberOfLevels);
	registration->SetSmoothingSigmasPerLevel(smoothingSigmasPerLevel);
	registration->SetShrinkFactorsPerLevel(shrinkFactorsPerLevel);
	registration->InPlaceOn();

	try
	{
		registration->Update();
		TransformType::ConstPointer transform = registration->GetTransform();
		TransformType::ParametersType finalParameters = transform->GetParameters();
		*xTranslation = finalParameters[0];
		*yTranslation = finalParameters[1];
		*numberOfIterations = optimizer->GetCurrentIteration();
		*quality = optimizer->GetValue();
	}
	catch (itk::ExceptionObject & err)
	{
		strcpy_s(error, charlength, err.what());
		return 1; 
	}
	return 0;
}