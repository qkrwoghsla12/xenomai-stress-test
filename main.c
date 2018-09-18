/*
 *  This file is owned by the Embedded Systems Laboratory of Seoul National University of Science and Technology
 *  to test EtherCAT master protocol using the IgH EtherCAT master userspace library.	
 *  
 *  
 *  IgH EtherCAT master library for Linux is found at the following URL: 
 *  <http://www.etherlab.org/en/ethercat>
 *
 *
 *
 *
 *  2015 Raimarius Delgado
*/
/****************************************************************************/
#include "main.h"
/****************************************************************************/
int  XenoInit(void);
void XenoQuit(void);
void DoInput();
void SignalHandler(int signum);
void PrintEval(int BufPrd[], int BufExe[], int BufJit[], int ArraySize);
void FilePrintEval(char *FileName,int BufPrd[], int BufExe[], int BufJit[], int BufCollect[], int BufProcess[], int BufTranslate[], int ArraySize);
/****************************************************************************/
int cpuspintime = 0;

#ifdef CPUSPIN
void CpuSpinTask(void *arg){
	while (1){
	rt_task_wait_period(NULL);
	rt_timer_spin(cpuspintime);
	}
}
#endif

void EcatCtrlTask(void *arg){
	int iSlaveCnt;
	RtmEcatPeriodEnd = rt_timer_read();

	while (1){
	rt_task_wait_period(NULL);
	RtmEcatPeriodStart = rt_timer_read();
	RtmCollectTime = RtmEcatPeriodStart;
	/*
	 * Do reading of the current process data from here
	 * before processing and sending to Tx
	 */
	/*Read EtherCAT Datagram*/

	#ifdef PROCESS
	volatile int i;
	volatile int ii;
	for(i=0;i<15000;i++){
		ii=ii+i*2;
	}
	#endif
	/* send process data */
	RtmEcatExecTime = rt_timer_read();
	RtmProcessTime =  RtmEcatExecTime;

	EcatPeriod 	 = ((int)RtmEcatPeriodStart - (int)RtmEcatPeriodEnd);
	EcatExecution = ((int)RtmEcatExecTime - (int)RtmEcatPeriodStart);
	EcatJitter	 = MathAbsValI(EcatPeriod - (int)ECATCTRL_TASK_PERIOD);
	EcatProcess	= (int)RtmProcessTime - (int)RtmCollectTime;
	//rt_printf("%d.%03d\n",EcatProcess/1000000, EcatProcess%1000000);
	//rt_printf("%d.%03d\n",EcatPeriod/1000000, EcatPeriod%1000000);

	BufEcatPeriodTime[iBufEcatDataCnt] 	=   EcatPeriod;
	BufEcatExecTime[iBufEcatDataCnt]  	=   EcatExecution;
	BufEcatJitter[iBufEcatDataCnt]  	=   EcatJitter;
	BufEcatCollect[iBufEcatDataCnt]  	=   EcatCollect;
	BufEcatProcess[iBufEcatDataCnt]  	=   EcatProcess;
	BufEcatTranslate[iBufEcatDataCnt]  	=   EcatTranslate;

	if(iBufEcatDataCnt == BUF_SIZE){
		bQuitFlag = TRUE;
	}else{
		++iBufEcatDataCnt;
	}

   	RtmEcatPeriodEnd = RtmEcatPeriodStart;
    }
}
/****************************************************************************/
int main(int argc, char **argv){
	int ret = 0;
	char *filename;
	if(argv[1]){
		cpuspintime = atoi(argv[1])*1000;
		filename = argv[2];
	}
	else{
		printf("usage : ./start.sh cpuspintime(us) filename");
	}
	/* Interrupt Handler "ctrl+c"  */
	signal(SIGTERM, SignalHandler);
        signal(SIGINT, SignalHandler);

	mlockall(MCL_CURRENT|MCL_FUTURE); //Lock Memory to Avoid Memory Swapping

	/* RT-task */
	if ((ret = XenoInit())!=0){
		fprintf(stderr, "Failed to Initiate Xenomai Services!\n");
		return ret;
	}

	while (1) {
		usleep(1);
		if (bQuitFlag) break;
	}

	FilePrintEval((char *)filename,BufEcatPeriodTime,
			BufEcatExecTime,BufEcatJitter,BufEcatCollect, BufEcatProcess, BufEcatTranslate,iBufEcatDataCnt);

	PrintEval(BufEcatPeriodTime,BufEcatExecTime,BufEcatJitter,iBufEcatDataCnt);

	XenoQuit();

return ret;
}

/****************************************************************************/
void SignalHandler(int signum){
		bQuitFlag = TRUE;
}

/****************************************************************************/

int XenoInit(void){

	rt_print_auto_init(1); //RTDK

	printf("Creating Xenomai Realtime Task(s)...");
	if(rt_task_create(&TskEcatCtrl,"EtherCAT Control", 0, ECATCTRL_TASK_PRIORITY,ECATCTRL_TASK_MODE)){
      		fprintf(stderr, "Failed to create Ecat Control Task\n");
		return _EMBD_RET_ERR_;
	}

	#ifdef CPUSPIN
	if(rt_task_create(&CpuSpin,"Cpu Spin", 0, CPUSPIN_TASK_PRIORITY, ECATCTRL_TASK_MODE)){
      		fprintf(stderr, "Failed to create Cpu Spin Task\n");
		return _EMBD_RET_ERR_;
	}
	#endif
	#ifdef PROCESS
		printf("add process\n");
	#endif
	printf("OK!\n");


	printf("Making Realtime Task(s) Periodic...\n");
	if(rt_task_set_periodic(&TskEcatCtrl, TM_NOW,rt_timer_ns2ticks(ECATCTRL_TASK_PERIOD))){
		fprintf(stderr, "Failed to Make Ecat Control Task Periodic\n");
		return _EMBD_RET_ERR_;
	}

	printf("Starting Xenomai Realtime Task EcatCtrl...\n");
	if(rt_task_start(&TskEcatCtrl, &EcatCtrlTask, NULL)){
		fprintf(stderr, "Failed to start Ecat Control Task\n");
		return _EMBD_RET_ERR_;
	}

	#ifdef CPUSPIN
	if(rt_task_set_periodic(&CpuSpin, TM_NOW, rt_timer_ns2ticks(ECATCTRL_TASK_PERIOD))){
		fprintf(stderr, "Failed to Make Cpu Spin Task Periodic\n");
		return _EMBD_RET_ERR_;
	}

	printf("Starting Xenomai Realtime Task CpuSpin...\n");
	printf("Spin Time : %d\n",cpuspintime);
	if(rt_task_start(&CpuSpin, &CpuSpinTask, NULL)){
		fprintf(stderr, "Failed to start Cpu Spin Task\n");
		return _EMBD_RET_ERR_;
	}
	#endif
	printf("OK!\n");

	return _EMBD_RET_SCC_;
}

/****************************************************************************/

void XenoQuit(void){
	rt_task_suspend(&TskEcatCtrl);
	rt_task_delete(&TskEcatCtrl);
	#ifdef CPUSPIN
	rt_task_suspend(&CpuSpin);
	rt_task_delete(&CpuSpin);
	#endif
	printf("\033[%dm%s\033[0m",95,"Xenomai Task(s) Deleted!\n");
}

/****************************************************************************/

void PrintEval(int BufPrd[], int BufExe[], int BufJit[], int ArraySize){

/* MATH_STATS: Simple Statistical Analysis (libs/embedded/embdMATH.h)
 * 	float ave;
 * 	float max;
 * 	float min;
 * 	float std; */
	MATH_STATS EcatPeriodStat, EcatExecStat, EcatJitterStat;

	EcatPeriodStat = GetStatistics(BufPrd, ArraySize,SCALE_1M);  
	printf("\n[Period] Max: %.6f Min: %.6f Ave: %.6f St. D: %.6f\n", EcatPeriodStat.max,
			EcatPeriodStat.min,
			EcatPeriodStat.ave,
			EcatPeriodStat.std);
	EcatExecStat = GetStatistics(BufExe, ArraySize,SCALE_1K);  
	printf("[Exec]	 Max: %.3f Min: %.3f Ave: %.3f St. D: %.3f\n", EcatExecStat.max,
			EcatExecStat.min,
			EcatExecStat.ave,
			EcatExecStat.std);
	EcatJitterStat = GetStatistics(BufJit, ArraySize,SCALE_1K);  
	printf("[Jitter] Max: %.3f Min: %.3f Ave: %.3f St. D: %.3f\n", EcatJitterStat.max,
			EcatJitterStat.min,
			EcatJitterStat.ave,
			EcatJitterStat.std);
}

/****************************************************************************/

void FilePrintEval(char *FileName,int BufPrd[], int BufExe[], int BufJit[], int BufCollect[], int BufProcess[], int BufTranslate[], int ArraySize){

	FILE *FileEcatTiming;
	int iCnt;

	FileEcatTiming = fopen(FileName, "w");

	for(iCnt=0; iCnt < ArraySize; ++iCnt){
		fprintf(FileEcatTiming,"%d.%06d,%d.%03d,%d.%03d,%d.%03d,%d.%03d,%d.%03d\n",
				BufPrd[iCnt]/SCALE_1M,
				BufPrd[iCnt]%SCALE_1M,
				BufExe[iCnt]/SCALE_1K,
				BufExe[iCnt]%SCALE_1K,
				BufJit[iCnt]/SCALE_1K,
				BufJit[iCnt]%SCALE_1K,
				BufCollect[iCnt]/SCALE_1K,
				BufCollect[iCnt]%SCALE_1K,
				BufProcess[iCnt]/SCALE_1K,
				BufProcess[iCnt]%SCALE_1K,
				BufTranslate[iCnt]/SCALE_1K,
				BufTranslate[iCnt]%SCALE_1K);
	}
	fclose(FileEcatTiming);
}

/****************************************************************************/
