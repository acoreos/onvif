
/*******************************************************************+
 *                             _ooOoo_								*
 *                            o8888888o								*
 *                            88" . "88								*
 *                            (| -_- |)								*
 *                            O\  =  /O								*
 *                         ____/`---'\____							*
 *                       .'  \\|     |//  `.						*
 *                      /  \\|||  :  |||//  \						*
 *                     /  _||||| -:- |||||-  \						*
 *                     |   | \\\  -  /// |   |						*
 *                     | \_|  ''\---/''  |   |						*
 *                     \  .-\__  `-`  ___/-. /						*
 *                   ___`. .'  /--.--\  `. . __						*
 *                ."" '<  `.___\_<|>_/___.'  >'"".					*
 *               | | :  `- \`.;`\ _ /`;.`/ - ` : | |				*
 *               \  \ `-.   \_ __\ /__ _/   .-` /  /				*
 *          ======`-.____`-.___\_____/___.-`____.-'======			*
 *                             `=---='								*
 *          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^			*
 *                     佛祖保佑        永无BUG						*
 *																	*
 +******************************************************************/

/************************************************************************
**
** 作者：梁友
** 日期：2018-07-02
** 描述：第一阶段提供的demo
**
************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/************************************************************************
**函数：getNowTime
**功能：获取当前时间
**参数：
        [out]  current - 返回当前时间的字符串 (如 2018-07-02 11:28)
**返回：
		返回当前时间的 time_t 结构体
************************************************************************/
static time_t getNowTime(char *current)
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);  //获取相对于1970到现在的秒数
    struct tm nowTime;
    localtime_r(&time.tv_sec, &nowTime);
    sprintf(current, "%04d-%02d-%02d %02d:%02d", nowTime.tm_year + 1900, nowTime.tm_mon+1, nowTime.tm_mday, 
              nowTime.tm_hour, nowTime.tm_min);
	return mktime(&nowTime);
}

/************************************************************************
**函数：getFileNum_FindMinFile
**功能：获取指定目录的普通文件个数，并且返回“最小“的文件名
**参数：
        [in]   root - 查找的目录
        [out]  old_file - 该目录下”最小“文件名的字符串
**返回：
		制定目录下的普通文件个数
************************************************************************/
static int getFileNum_FindMinFile(char* root, char *old_file)
{
    int total = 0;
    DIR* dir = NULL;
    // 打开目录
    dir = opendir(root);
	if (dir == NULL) {
		perror("opendir");
		return -1;
	}

    char path[1024];
	char tmp[512];
    // 定义记录目录指针
    struct dirent* ptr = NULL;
    while( (ptr = readdir(dir)) != NULL)
    {
        // 跳过. 和 ..
        if(strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
        {
            continue;
        }
        // 如果是普通文件
        if(ptr->d_type == DT_REG)
        {
			if (total == 0)
				strcpy(old_file, ptr->d_name);

			if (strcmp(old_file, ptr->d_name) > 0)
				strcpy(old_file, ptr->d_name);
            total ++;
        }
    }

    closedir(dir);
    return total;
}

#define DEF_VIDEO_DIR "/home/ftp/video"

#ifndef DEF_VIDEO_DIR
#error "undef DEF_VIDE_DIR"
#endif
FILE *get_valied_fp(void)
{
	static time_t tmp = 0;
	static char filename[512] = {0};
	static char time_str[32] = {0};
	static time_t cur_time;
	static int num  = 0; 
	static FILE *file;
	char base_dir[32] = {0};

	strcpy(base_dir, DEF_VIDEO_DIR);
	cur_time = getNowTime(time_str);
	
	if ((cur_time - tmp) >=  60) {

		if (strlen(filename) != 0)  {
			printf("close %s\n", filename);
			fclose(file);
		}

		num = getFileNum_FindMinFile(base_dir, filename);
		if (num >= 10) {
			char remove_file[512] = {0};
			sprintf(remove_file, "%s/%s", base_dir, filename);
			printf("remove %s\n", remove_file);
			if (remove(remove_file) == -1) {
				perror("file");
			}
		} 

		sprintf(filename, "%s/%s", base_dir, time_str);
		printf("open %s\n", filename);
		file = fopen(filename, "w+");
		if (NULL == file) {
			printf("open %s err!\n", filename);
		}
		tmp = cur_time;
	}

	return file;
}


