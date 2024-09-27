#include "src/CVCTInterface.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

__asm__(".symver __libc_start_main,__libc_start_main@GLIBC_2.17");


static status_callback(char* jsonstr) {
	printf("------------status notify------------------\n");
	puts(jsonstr);
}

int main(int argc, char *argv[])
{
 

 
	char* configfile = "test.json";
	char* fakekey = "58lF4jRz.ITxj79aIheN0doWBFZHZLV4HF9KSbFSJ";

	if ((argc % 2) == 0) return 0;

	for (int i = 1; i < argc; i += 2)
	{
		if (strlen(argv[i]) != 2) return 0;
		switch (argv[i][1])
		{
		case 'c':
			configfile = argv[i + 1];
			break;

		case 'k':
			fakekey = argv[i + 1];
			break;
		default: return 0;
		}
	}
	
	 puts(TS_queryLicense());
	// return;
 

	err_sdk_code ret = TS_Init(NULL, 2, status_callback);

	if (ret != ERR_NO_ERROR) {
		puts("init failed-------------------");
		return 0;
	}


	char buf[4096] = { 0 };

	FILE* config = fopen(configfile, "r");
	fread(buf, 1, 4096, config);
	fclose(config);

	puts("read config ok");

	cJSON * root = cJSON_Parse(buf);
	cJSON *tList = cJSON_GetObjectItem(root, "tasklist");
	if (!tList) {
		printf("no json\n");
		return -1;
	}
puts("-------main ---------------   3");

	int array_size = cJSON_GetArraySize(tList);
	cJSON *item;
	cJSON *name;
	printf("---- total task count = %d\n", array_size);
	for (int i = 0; i < array_size; i++) {
		char* tmpbuf;
		item = cJSON_GetArrayItem(tList, i);
		name = cJSON_GetObjectItem(item, "taskID");

		//	printf("start task:%s\t", name->valuestring);
		tmpbuf = cJSON_Print(item);
		puts(tmpbuf);
		TS_createTask(tmpbuf);
		sleep(1);
		free(tmpbuf);

	}

	for (int i = 0; i < 10000; i++) {
		sleep(1);
		printf("wait............... %d second\n", i);

	}

	for (int i = 0; i < array_size; i++) {
		item = cJSON_GetArrayItem(tList, i);
		name = cJSON_GetObjectItem(item, "taskID");

		printf("delete task:%s\t", name->valuestring);
		TS_deleteTask(name->valuestring);
		
		sleep(1);

	}
	
	for (int i = 0; i < 1000; i++) {
		sleep(1);
		printf("idle............... %d second\n", i);

	}
	puts("=============release================");
	TS_Release();

	return 0;
}
