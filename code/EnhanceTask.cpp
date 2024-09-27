#if defined(NO_ENHANCE)
#include "EnhanceTask.h"
#include "TaskManager.h"
#include "RtspTask.h"
#include "TaskFlow.h"
#include "../common/ts_time.h"
EnhanceTask::EnhanceTask(TaskFlow* pFlow)
{
	
}

EnhanceTask::~EnhanceTask() {
	puts("~delete EnhanceTask");



}
void* EnhanceTask::startThread(TaskFlow* pFlow)
{
	EnhanceTask* thread = new EnhanceTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();
	return thread;
}
bool EnhanceTask::onStart()
{
	return true;
}
void EnhanceTask::stop()
{
	puts("------stop EnhanceTask-----");
}
void EnhanceTask::run()
{

}


#else

#include "EnhanceTask.h"
#include "TaskManager.h"
#include "RtspTask.h"
#include "RtspTask1.h"

#include "TaskFlow.h"
#include "../common/ts_time.h"
#include "nvCVOpenCV.h"
#include <opencv2/imgproc/imgproc_c.h>
#include "AudioFilterTask.h"
#include "../common/EdUrlParser.h"


EnhanceTask::EnhanceTask(TaskFlow* pFlow)
{
	pFlow_ = pFlow;
	puts("create EnhanceTask");
	vChain[0] = NULL;
	vChain[1] = NULL;
}

EnhanceTask::~EnhanceTask() {
	puts("~delete EnhanceTask");
	if (state)  cudaFree(state);
	if (_eff)
		NvVFX_DestroyEffect(_eff);
	if (vChain[0]) delete vChain[0];
	if (vChain[1]) delete vChain[1];


}
void* EnhanceTask::startThread(TaskFlow* pFlow)
{
	EnhanceTask* thread = new EnhanceTask(pFlow);

	thread->setAutoDelete(true);
	thread->start();
	return thread;
}
bool EnhanceTask::onStart()
{
	return true;
}
void EnhanceTask::stop()
{
	puts("------stop EnhanceTask-----");
}

int EnhanceTask::buildFilter()
{

	NvCV_Status vfxErr, vfxErr1, vfxErr2;

	(vfxErr = NvVFX_CreateEffect("Denoising", &effectHandle));

	(vfxErr1 = NvVFX_SetString(effectHandle, NVVFX_MODEL_DIRECTORY,pFlow_->vfxPath_.c_str()));
	printf("------   create filter  ---------------1  %d  %d\n", vfxErr, vfxErr1);

	vfxErr = NvVFX_CudaStreamCreate(&stream);
	vfxErr1 = NvVFX_SetCudaStream(effectHandle, NVVFX_CUDA_STREAM, stream);	printf("------   create filter  ---------------2 %d %d\n", vfxErr, vfxErr1);	const char* cStr;
	NvCV_Status err = NvVFX_GetString(effectHandle, NVVFX_INFO, &cStr);
	//NvCV_Status err = NvVFX_GetString(NULL, NVVFX_INFO, &cStr);
	if (NVCV_SUCCESS != err)
		printf("Cannot get effects: %s\n", NvCV_GetErrorStringFromCode(err));
	printf("where effects are:\n%s", cStr);	unsigned int stateSizeInBytes = 8;
	vfxErr = NvVFX_GetU32(effectHandle, "NVVFX_STATE_SIZE", &stateSizeInBytes);	printf("------   create filter  ------------3 statsize = %d %d %s\n", stateSizeInBytes, vfxErr, NvCV_GetErrorStringFromCode(vfxErr));
	cudaMalloc(&state, stateSizeInBytes);
	cudaMemsetAsync(state, 0, stateSizeInBytes, stream);
	stateArray[0] = state;
	(vfxErr = NvVFX_SetObject(effectHandle, NVVFX_STATE, (void*)stateArray));

	/*cudaMalloc(&state[0], stateSizeInBytes);	cudaMemset(state[0], 0, stateSizeInBytes);	vfxErr = NvVFX_SetObject(effectHandle, NVVFX_STATE, (void*)state);*/	printf("------   create filter  ---------------4 %d\n", vfxErr);
	return 0;
}

void EnhanceTask::processDenoise()
{
	puts(" --------- enchance run  ---------------");
	if (buildFilter() != 0) {
		puts("----- mx filter build failed ---");
		return;
	}

	NvCV_Status vfxErr, vfxErr1, vfxErr2;

	while (1) {
		Packet* frame = pFlow_->pRawData_in->trypop(10);

		if (bgrbuf == NULL) {
			puts("create-------bgr buf");
			bgrbuf = (unsigned char*)malloc(frame->width* frame->height * 3);
			dstbgrbuf = (unsigned char*)malloc(frame->width* frame->height * 3);
		}

		if (frame != NULL && frame->size > 0) {
			//set buffer
			(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, frame->width, frame->height, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
			(vfxErr1 = NvCVImage_Alloc(&_dstGpuBuf, frame->width, frame->height, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1)); //dst GPU


			printf("-----------enhance------1 %d %d\n", vfxErr, vfxErr1);
			(vfxErr = NvVFX_SetImage(effectHandle, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
			(vfxErr1 = NvVFX_SetImage(effectHandle, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
			(vfxErr2 = NvVFX_SetF32(effectHandle, NVVFX_STRENGTH, FLAG_strength));
			printf("-----------enhance------2 %d %d %d\n", vfxErr, vfxErr1, vfxErr2);




			NvCVImage srcCpuImg(frame->width, frame->height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU, 1);
			//NvCVImage srcCpuImg(frame->width, frame->height, NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU, 1);

			NvCVImage dstCpuImg(frame->width, frame->height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU, 1);
			//NvCVImage dstCpuImg(frame->width, frame->height, NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU, 1);
			//(vfxErr = NvCVImage_Alloc(&srcCpuImg, frame->width, frame->height, NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU, 1));  // src GPU
			//(vfxErr1 = NvCVImage_Alloc(&dstCpuImg, frame->width, frame->height, NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU, 1)); //dst GPU			(vfxErr2 = NvCVImage_Alloc(&dstCpuImg, frame->width, frame->height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU, 1)); //dst GPU
			//NvCVImage stageImg(srcCpuImg.width, srcCpuImg.height, srcCpuImg.pixelFormat, srcCpuImg.componentType, srcCpuImg.planar, NVCV_GPU);			NvCVImage stageImg;			puts("-----------enhance------4");
			vfxErr = NvVFX_Load(effectHandle);
			printf("-----------enhance------3 %d\n", vfxErr);
			vfxErr1 = NvCVImage_Init(&srcCpuImg, frame->width, frame->height, frame->width * 3, bgrbuf, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU);
			vfxErr2 = NvCVImage_Init(&dstCpuImg, frame->width, frame->height, frame->width * 3, dstbgrbuf, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU);
			printf("-----------enhance------3.5 %d %d\n", vfxErr1, vfxErr2);

			while (1) {
				/*vfxErr1 = NvCVImage_TransferFromYUV(frame->pdata_, frame->width, frame->height,
					frame->pdata_+frame->width*frame->height, frame->pdata_+frame->width*frame->height + 1,frame->width, frame->height / 2,
					NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU,
					&srcCpuImg, NULL,1.f, stream, &stageImg);*/
					// trans data to gpu

			//	vfxErr1 = NvCVImage_Init(&_srcGpuBuf, frame->width, frame->height, frame->width, frame->pdata_, NVCV_YUV420, NVCV_U8, NVCV_NV12, NVCV_CPU);

				NV12_To_RGB(frame->width, frame->height, (unsigned char*)frame->pdata_, bgrbuf);

				vfxErr = NvCVImage_Transfer(&srcCpuImg, &_srcGpuBuf, 1.f / 255.f, stream, &stageImg);
				printf("-----------enhance------5 %d %d\n", vfxErr, vfxErr1);
				vfxErr = NvVFX_Run(effectHandle, 0);
				printf("-----------enhance------6 %d %s\n", vfxErr, NvCV_GetErrorStringFromCode(vfxErr));
				vfxErr1 = NvCVImage_Transfer(&_dstGpuBuf, &dstCpuImg, 255.0f, stream, &stageImg);
				printf("-----------enhance------7 %d \n", vfxErr1);

				return;
				FILE * yuvout = fopen("newYuv0518.yuv", "w");
				fwrite(dstCpuImg.pixels, frame->size, 1, yuvout);
				fclose(yuvout);
				puts("-----------enhance------8");


				//run filter;
				// get output data
				/*if (pFlow_->pRawData->size() < 10 && frame->size > 0) {

					Packet* newItem = new Packet(frame->pdata_, frame->size);
					newItem->width = frame->width;
					newItem->height = frame->height;
					newItem->size = frame->size;
					pFlow_->pRawData->push(newItem);
				}*/
				frame = pFlow_->pRawData_in->pop();

			}
		}
		else
		{
			puts("------eos EnhanceTask-----");
			return;

		}
	}


}
void EnhanceTask::processDenoiseCV(char* effc)
{
	cv::Mat yuvImg;
	NvCV_Status   vfxErr;
	int index = 0;

	while (1) {
		Packet* frame = pFlow_->pRawData_in->trypop(10);
		if (frame != NULL && frame->size > 0) {

			if (!chainInited) {

				yuvImg.create(frame->height * 3 / 2, frame->width, CV_8UC1);
				memcpy(yuvImg.data, frame->pdata_, frame->len_);
				cv::cvtColor(yuvImg, _srcImg, CV_YUV420sp2RGB);
			//	puts("--- handle cv ------------2 ");

				buildFilter1(effc);
				chainInited = true;
			}
			else {
				memcpy(yuvImg.data, frame->pdata_, frame->len_);
				cv::cvtColor(yuvImg, _srcImg, CV_YUV420sp2RGB);
			}

			vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf, 1.f , stream, &_tmpVFX);
			if (vfxErr != 0) {
				printf("----------12 %d\n", vfxErr);
			}

			vfxErr = NvVFX_Run(_eff, 0);
			if (vfxErr != 0) {
				printf("----------19 %d\n", vfxErr);
			}

			(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, 1.f, stream, &_tmpVFX));
			if (vfxErr != 0)printf("----------20 %d\n", vfxErr);

		
			{
				cv::Mat yuvImg1;
				cv::cvtColor(_dstImg, yuvImg1, CV_BGR2YUV_I420);

				if (pFlow_->pRawData->size() < 10) {

					Packet* newItem = new Packet(yuvImg1.data, yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1());
					newItem->width = frame->width;
					newItem->height = frame->height;
					newItem->size = yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1();
					newItem->pts_ = frame->pts_;
					newItem->seq_ = frame->seq_;
					newItem->type_ = frame->type_;
					pFlow_->pRawData->push(newItem);
					//	puts("+++++++++ add I420 data");
				}
			
			/*	if (!cv::imwrite("out1.jpg", _dstImg)) {
				return;
			}*/
			/*	char fnamebuf[128] = { 0 };
				sprintf(fnamebuf, "yuv_%d.yuv", index++);
				FILE * yuvout = fopen(fnamebuf, "w");
				fwrite(yuvImg1.data, yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1(), 1, yuvout);
				fclose(yuvout);
			*/
			}

			delete frame;
		}
		else
		{
			puts("------eos EnhanceTask after 10 s-----");
			return;
		}
	}
}

void EnhanceTask::processChain(char* a, char* b)
{
	vChain[0] = new VideoFilter(a,pFlow_->vfxPath_);
	vChain[1] = new VideoFilter(b, pFlow_->vfxPath_);
	int index = 0;
	cv::Mat yuvImg;
	while (1) {
		Packet* frame = pFlow_->pRawData_in->trypop(10);
		if (frame != NULL && frame->size > 0) {
			if (!chainInited) {
				initChain(frame);
				yuvImg.create(frame->height * 3 / 2, frame->width, CV_8UC1);
				chainInited = true;
			}

			memcpy(yuvImg.data, frame->pdata_, frame->len_);
			cv::cvtColor(yuvImg, vChain[0]->_srcImg, CV_YUV420sp2RGB);

			vChain[0]->processOneFrame();

			vChain[1]->processOneFrame();

			cv::Mat yuvImg1;

			vChain[1]->outputYUV(yuvImg1);


			if (pFlow_->pRawData->size() < 10) {

				Packet* newItem = new Packet(yuvImg1.data, yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1());
				newItem->width = frame->width;
				newItem->height = frame->height;
				newItem->size = yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1();
				newItem->pts_ = frame->pts_;
				newItem->seq_ = frame->seq_;
				newItem->type_ = frame->type_;
				pFlow_->pRawData->push(newItem);
				//	puts("+++++++++ add I420 data");
			}

			/*	char fnamebuf[128] = { 0 };
				sprintf(fnamebuf, "yuv_new_%d.yuv", index++);
				FILE * yuvout = fopen(fnamebuf, "w");
				fwrite(yuvImg1.data, yuvImg1.rows * yuvImg1.cols * yuvImg1.elemSize1(), 1, yuvout);
				fclose(yuvout);
			*/
			/*	if (index++ > 2) {
					exit(0);
				}*/

			delete frame;

		}
		else {
			Packet* newItem;
			pFlow_->pRawData->push(newItem);
		}
	}

}
int EnhanceTask::initChain(Packet* frame)
{
	vChain[0]->init(frame);

	vChain[1]->init(frame);

	vChain[1]->upLink(vChain[0]);

	chainInited = true;

	return 0;
}

int EnhanceTask::buildFilter1(char* effname)
{

	NvCV_Status   vfxErr;
	/*void* state = nullptr;
	void* stateArray[1];
*/
	if (strcmp(effname, "D") == 0)
		(vfxErr = NvVFX_CreateEffect("Denoising", &_eff));
	else 	if (strcmp(effname, "A") == 0)
		(vfxErr = NvVFX_CreateEffect(NVVFX_FX_ARTIFACT_REDUCTION, &_eff));
	(vfxErr = NvVFX_SetString(_eff, NVVFX_MODEL_DIRECTORY,  pFlow_->vfxPath_.c_str()));

	/*vfxErr = NvVFX_CudaStreamCreate(&stream);
	vfxErr = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, stream);	printf("------   create filter  ---------------2 %d \n", vfxErr);*/
	if (!_eff)
		return -1;

	//puts("--- handle cv ------------3 ");

	(vfxErr = allocBuffers1(_srcImg.cols, _srcImg.rows));
	if (0 != vfxErr)printf("----------1 %d\n", vfxErr);

	//puts("--- handle cv ------------4 ");


	(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
	if (0 != vfxErr)printf("----------13 %d\n", vfxErr);
	(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
	if (0 != vfxErr)printf("----------14 %d\n", vfxErr);
	(vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, FLAG_strength));
	if (0 != vfxErr)printf("----------15 %d\n", vfxErr);

	unsigned int stateSizeInBytes;
	(vfxErr = NvVFX_GetU32(_eff, NVVFX_STATE_SIZE, &stateSizeInBytes));
	printf("----------16 %d %d\n", vfxErr, stateSizeInBytes);

	cudaMalloc(&state, stateSizeInBytes);
	puts("--- handle cv ------------1 ");

	cudaMemsetAsync(state, 0, stateSizeInBytes, stream);
	puts("--- handle cv ------------5 ");

	stateArray[0] = state;
	(vfxErr = NvVFX_SetObject(_eff, NVVFX_STATE, (void*)stateArray));
	if (0 != vfxErr)printf("----------17 %d\n", vfxErr);

	(vfxErr = NvVFX_Load(_eff));
	if (0 != vfxErr)printf("----------18 %d\n", vfxErr);

	return 0;
}

void EnhanceTask::run()
{
	if (pFlow_->withaudio_ == 1) {
		AudioFilter::startThread(pFlow_);
	}
	string veCfg = pFlow_->vEnhanceMode_;// "A:D";

	vector<string> list;
	EdUrlParser::split(veCfg, ":", &list);
	if (list.size() == 1) {
		printf("===================start handle filter:%s %s==================\n", veCfg.c_str(), list[0].c_str());
		processDenoiseCV((char*)list[0].c_str());
	}
	else if (list.size() >= 2) {
		printf("===================start handle filter:%s %s %s==================\n", veCfg.c_str(), list[0].c_str(), (char*)list[1].c_str());
		processChain((char*)list[0].c_str(), (char*)list[1].c_str());
	}

	//processImage("input_003054.jpg", "out.jpg");

}

VideoFilter::VideoFilter() {

}

VideoFilter::VideoFilter(char* effc,string& modpath) {
	if (strcmp(effc, "D") == 0)
		effcname = "Denoising";
	else if (strcmp(effc, "A") == 0)
		effcname = "ArtifactReduction";

	modelpath_ = modpath;

}

VideoFilter::~VideoFilter() {
	if (state)  cudaFree(state);
	if (_eff)
		NvVFX_DestroyEffect(_eff);
}
int VideoFilter::init(Packet* frame) {
	NvCV_Status   vfxErr;

	printf("------init filter---%s-----------\n", effcname.c_str());

	(vfxErr = NvVFX_CreateEffect(effcname.c_str(), &_eff));
	if (vfxErr != 0)printf("sub filter ----------------- err=%d\n", vfxErr);
	(vfxErr = NvVFX_SetString(_eff, NVVFX_MODEL_DIRECTORY, modelpath_.c_str()));
	if (vfxErr != 0)printf("sub filter -----------------1 err=%d\n", vfxErr);

	if (!_eff)
		return -1;

	cv::Mat yuvImg;
	yuvImg.create(frame->height * 3 / 2, frame->width, CV_8UC1);
	memcpy(yuvImg.data, frame->pdata_, frame->len_);
	cv::cvtColor(yuvImg, _srcImg, CV_YUV420sp2RGB);


	if (!_srcImg.data) {
		_srcImg.create(_srcImg.cols, _srcImg.rows, CV_8UC3);                                                                                        // src CPU
	}
	_dstImg.create(_srcImg.rows, _srcImg.cols, _srcImg.type()); // 


//	(_dstImg.data, vfxErr, NVCV_ERR_MEMORY); // 
	(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
	if (vfxErr != 0)printf("sub filter ----------------2- err=%d\n", vfxErr);

	(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1)); //dst GPU
	if (vfxErr != 0)printf("sub filter -----------------3 err=%d\n", vfxErr);

	NVWrapperForCVMat(&_srcImg, &_srcVFX);      // _srcVFX is an alias for _srcImg
	NVWrapperForCVMat(&_dstImg, &_dstVFX);      // _dstVFX is an alias for _dstImg

	(vfxErr = NvCVImage_Alloc(&_tmpVFX, _dstVFX.width, _dstVFX.height, _dstVFX.pixelFormat, _dstVFX.componentType, _dstVFX.planar, NVCV_GPU, 0));
	if (vfxErr != 0)printf("sub filter -----------------4 err=%d\n", vfxErr);

	(vfxErr = NvCVImage_Realloc(&_tmpVFX, _srcVFX.width, _srcVFX.height, _srcVFX.pixelFormat, _srcVFX.componentType, _srcVFX.planar, NVCV_GPU, 0));
	if (vfxErr != 0)printf("sub filter -----------------5 err=%d\n", vfxErr);



	(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
	if (vfxErr != 0)printf("sub filter -----------------6 err=%d\n", vfxErr);
	(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
	if (vfxErr != 0)printf("sub filter -----------------7 err=%d\n", vfxErr);
	//	(vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, FLAG_strength));
	//	printf("f----------15 %d\n", vfxErr);

	unsigned int stateSizeInBytes;
	(vfxErr = NvVFX_GetU32(_eff, NVVFX_STATE_SIZE, &stateSizeInBytes));
	if (vfxErr != 0)printf("sub filter -----------------8 err=%d %d\n", vfxErr, stateSizeInBytes);

	cudaMalloc(&state, stateSizeInBytes);
	cudaMemsetAsync(state, 0, stateSizeInBytes, stream);
	stateArray[0] = state;
	(vfxErr = NvVFX_SetObject(_eff, NVVFX_STATE, (void*)stateArray));
	if (vfxErr != 0)printf("sub filter -----------------9 err=%d\n", vfxErr);


	(vfxErr = NvVFX_Load(_eff));
	if (vfxErr != 0)printf("sub filter -----------------10 err=%d\n", vfxErr);

	return 0;
}
//------------------------------tool func -------------------------------------------

int VideoFilter::processOneFrame() {
	NvCV_Status   vfxErr;
	(vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf, 1.f / 255.f, stream, &_tmpVFX));
	if (vfxErr != 0)printf("sub filter run ----------------- err=%d\n", vfxErr);
	(vfxErr = NvVFX_Run(_eff, 0));
	if (vfxErr != 0)printf("sub filter run -----------------1 err=%d\n", vfxErr);
	(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, 255.f, stream, &_tmpVFX));
	if (vfxErr != 0)printf("sub filter run -----------------2 err=%d\n", vfxErr);
}
int VideoFilter::outputYUV(cv::Mat& mat)
{
	cv::cvtColor(_dstImg, mat, CV_BGR2YUV_I420);
	return 0;
}
int VideoFilter::upLink(VideoFilter* upper)
{
	NVWrapperForCVMat(&upper->_dstImg, &_srcVFX);      // _srcVFX is an alias for _srcImg
	return 0;
}
int VideoFilter::downLink(VideoFilter*) {
	return 0;
}
void VideoFilter::release()
{
	//if (_eff)
	//	NvVFX_DestroyEffect(_eff);
	//_eff = NULL;
}

NvCV_Status EnhanceTask::allocBuffers1(unsigned width, unsigned height) {
	NvCV_Status  vfxErr = NVCV_SUCCESS;

	if (_inited)
		return NVCV_SUCCESS;

	if (!_srcImg.data) {
		_srcImg.create(height, width, CV_8UC3);                                                                                        // src CPU
		//BAIL_IF_NULL(_srcImg.data, vfxErr, NVCV_ERR_MEMORY);
	}
//	puts("--- handle cv ------------6 ");

	_dstImg.create(_srcImg.rows, _srcImg.cols, _srcImg.type()); // 
	(_dstImg.data, vfxErr, NVCV_ERR_MEMORY); // 
	(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
	(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1)); //dst GPU

	//puts("--- handle cv ------------7 ");

	NVWrapperForCVMat(&_srcImg, &_srcVFX);      // _srcVFX is an alias for _srcImg
	NVWrapperForCVMat(&_dstImg, &_dstVFX);      // _dstVFX is an alias for _dstImg

	//#define ALLOC_TEMP_BUFFERS_AT_RUN_TIME    // Deferring temp buffer allocation is easier
#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME      // Allocating temp buffers at load time avoids run time hiccups
	(vfxErr = allocTempBuffers()); // This uses _srcVFX and _dstVFX and allocates one buffer to be a temporary for src and dst
#endif // ALLOC_TEMP_BUFFERS_AT_RUN_TIME

	_inited = true;

bail:
	return vfxErr;
}


NvCV_Status EnhanceTask::allocBuffers(unsigned width, unsigned height) {
	NvCV_Status  vfxErr = NVCV_SUCCESS;

	if (_inited)
		return NVCV_SUCCESS;

	if (!_srcImg.data) {
		_srcImg.create(height, width, CV_8UC3);                                                                                        // src CPU
		//BAIL_IF_NULL(_srcImg.data, vfxErr, NVCV_ERR_MEMORY);
	}

	_dstImg.create(_srcImg.rows, _srcImg.cols, _srcImg.type()); // 
	(_dstImg.data, vfxErr, NVCV_ERR_MEMORY); // 
	(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
	(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_BGR, NVCV_F32, NVCV_PLANAR, NVCV_GPU, 1)); //dst GPU

	NVWrapperForCVMat(&_srcImg, &_srcVFX);      // _srcVFX is an alias for _srcImg
	NVWrapperForCVMat(&_dstImg, &_dstVFX);      // _dstVFX is an alias for _dstImg

	//#define ALLOC_TEMP_BUFFERS_AT_RUN_TIME    // Deferring temp buffer allocation is easier
#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME      // Allocating temp buffers at load time avoids run time hiccups
	(vfxErr = allocTempBuffers()); // This uses _srcVFX and _dstVFX and allocates one buffer to be a temporary for src and dst
#endif // ALLOC_TEMP_BUFFERS_AT_RUN_TIME

	_inited = true;

bail:
	return vfxErr;
}

NvCV_Status EnhanceTask::allocTempBuffers() {
	NvCV_Status vfxErr;
	(vfxErr = NvCVImage_Alloc(&_tmpVFX, _dstVFX.width, _dstVFX.height, _dstVFX.pixelFormat, _dstVFX.componentType, _dstVFX.planar, NVCV_GPU, 0));
	(vfxErr = NvCVImage_Realloc(&_tmpVFX, _srcVFX.width, _srcVFX.height, _srcVFX.pixelFormat, _srcVFX.componentType, _srcVFX.planar, NVCV_GPU, 0));
bail:
	return vfxErr;
}

void EnhanceTask::yuv2bgr(Packet* frame) {

	int bgrpos = 0;
	for (int i = 0; i < frame->width; i++) {
		for (int j = 0; j < frame->height; j++) {
			bgrbuf[bgrpos] = 0;
			bgrbuf[bgrpos + 1] = 0;
			bgrbuf[bgrpos + 2] = 0;
		}
	}
}

bool EnhanceTask::RGB24_TO_YUV420(unsigned char *RgbBuf, int w, int h, unsigned char *yuvBuf)
{
	unsigned char*ptrY, *ptrU, *ptrV, *ptrRGB;
	memset(yuvBuf, 0, w*h * 3 / 2);
	ptrY = yuvBuf;
	ptrU = yuvBuf + w * h;
	ptrV = ptrU + (w*h * 1 / 4);
	unsigned char y, u, v, r, g, b;
	for (int j = 0; j < h; j++) {
		ptrRGB = RgbBuf + w * j * 3;
		for (int i = 0; i < w; i++) {

			r = *(ptrRGB++);
			g = *(ptrRGB++);
			b = *(ptrRGB++);
			y = (unsigned char)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
			u = (unsigned char)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
			v = (unsigned char)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
			//*(ptrY++) = clip_value(y, 0, 255);
			if (j % 2 == 0 && i % 2 == 0) {
				//	*(ptrU++) = clip_value(u, 0, 255);
			}
			else {
				if (i % 2 == 0) {
					//	*(ptrV++) = clip_value(v, 0, 255);
				}
			}
		}
	}
	return true;
}

bool EnhanceTask::yuv420ToRgb(char *yuv, int w, int h, char *rgb)
{
	unsigned char *pBufy = new unsigned char[w*h];
	unsigned char *pBufu = new unsigned char[w*h / 4];
	unsigned char *pBufv = new unsigned char[w*h / 4];

	memcpy(pBufy, yuv, w*h);
	memcpy(pBufu, yuv + w * h, w*h / 4);
	memcpy(pBufv, yuv + w * h * 5 / 4, w*h / 4);

	for (int i = 0; i < w*h / 4; i++)
	{
		rgb[i * 3 + 2] = pBufy[i] + 1.772*(pBufu[i] - 128);  //B = Y +1.779*(U-128)
		rgb[i * 3 + 1] = pBufy[i] - 0.34413*(pBufu[i] - 128) - 0.71414*(pBufv[i] - 128);//G = Y-0.3455*(U-128)-0.7169*(V-128)
		rgb[i * 3 + 0] = pBufy[i] + 1.402*(pBufv[i] - 128);//R = Y+1.4075*(V-128)
	}
	free(pBufy);
	free(pBufu);
	free(pBufv);
	return true;
}

void EnhanceTask::NV12_To_RGB(unsigned int width, unsigned int height, unsigned char *yuyv, unsigned char *rgb)
{
	const int nv_start = width * height;
	int  i, j, index = 0, rgb_index = 0;
	unsigned char y, u, v;
	int r, g, b, nv_index = 0;


	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			//nv_index = (rgb_index / 2 - width / 2 * ((i + 1) / 2)) * 2;
			nv_index = i / 2 * width + j - j % 2;
			y = yuyv[rgb_index];
			// 			u = yuyv[nv_start + nv_index];
			// 			v = yuyv[nv_start + nv_index + 1];

			u = yuyv[nv_start + nv_index + 1];
			v = yuyv[nv_start + nv_index];

			r = y + (140 * (v - 128)) / 100;  //r
			g = y - (34 * (u - 128)) / 100 - (71 * (v - 128)) / 100; //g
			b = y + (177 * (u - 128)) / 100; //b

			if (r > 255)
				r = 255;
			if (g > 255)
				g = 255;
			if (b > 255)
				b = 255;
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			index = i * width + j;// rgb_index % width + (height - i - 1) * width;
			rgb[index * 3 + 0] = r;
			rgb[index * 3 + 1] = g;
			rgb[index * 3 + 2] = b;
			rgb_index++;
		}
	}
}


int EnhanceTask::processImage(const char *inFile, const char *outFile) {
	CUstream      stream = 0;
	NvCV_Status   vfxErr;

	//void* state = nullptr;
	//void* stateArray[1];
	(vfxErr = NvVFX_CreateEffect("Denoising", &_eff));
	(vfxErr = NvVFX_SetString(_eff, NVVFX_MODEL_DIRECTORY, pFlow_->vfxPath_.c_str()));


	if (!_eff)
		return -1;
	_srcImg = cv::imread(inFile);
	if (!_srcImg.data)
		return -2;
	puts("cv demo-----------------------1");
	(vfxErr = allocBuffers(_srcImg.cols, _srcImg.rows));

	if (vfxErr != 0)printf("----------1 %d\n", vfxErr);
	(vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf, 1.f / 255.f, stream, &_tmpVFX)); // _srcVFX--> _tmpVFX --> _srcGpuBuf
	if (vfxErr != 0)printf("----------12 %d\n", vfxErr);
	(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
	if (vfxErr != 0)printf("----------13 %d\n", vfxErr);
	(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
	if (vfxErr != 0)printf("----------14 %d\n", vfxErr);
	(vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, FLAG_strength));
	if (vfxErr != 0)printf("----------15 %d\n", vfxErr);

	unsigned int stateSizeInBytes;
	(vfxErr = NvVFX_GetU32(_eff, NVVFX_STATE_SIZE, &stateSizeInBytes));
	if (vfxErr != 0)printf("----------16 %d\n", vfxErr);
	cudaMalloc(&state, stateSizeInBytes);
	cudaMemsetAsync(state, 0, stateSizeInBytes, stream);
	stateArray[0] = state;
	(vfxErr = NvVFX_SetObject(_eff, NVVFX_STATE, (void*)stateArray));
	if (vfxErr != 0)printf("----------17 %d\n", vfxErr);

	(vfxErr = NvVFX_Load(_eff));
	if (vfxErr != 0)printf("----------18 %d\n", vfxErr);
	if (0 != (vfxErr = NvVFX_Run(_eff, 0)))
		printf("----------19 %d\n", vfxErr);
	(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, 255.f, stream, &_tmpVFX));
	if (vfxErr != 0)printf("----------20 %d\n", vfxErr);
	puts("cv demo-----------------------4");

	if (outFile && outFile[0]) {
		//if (IsLossyImageFile(outFile))
		//	fprintf(stderr, "WARNING: JPEG output file format will reduce image quality\n");
		if (!cv::imwrite(outFile, _dstImg)) {
			printf("Error writing: \"%s\"\n", outFile);
			return -3;
		}
	}
	/*if (_show) {
		cv::imshow("Output", _dstImg);
		cv::waitKey(3000);
	}*/
bail:
	if (state)  cudaFree(state); // release state memory
	return vfxErr;
}

#endif