#define _XOPEN_SOURCE 500
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <malloc.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>

time_t my_time;
struct tm * timeinfo;

#define PORT 8080
#define SIZE 1024

// SAFE_RUN
static jmp_buf context;
typedef void (*func_t)(void);

// GLOBAL_DEFINITION
bool GLOBAL_IS_ROOT = false;

char GLOBAL_CWD[PATH_MAX];
char GLOBAL_LOG_MSG[SIZE];
char GLOBAL_BUFFER[SIZE] = {0};
char GLOBAL_QUERY[SIZE];
char GLOBAL_REPLY_MSG[10*SIZE];

char GLOBAL_USERNAME[SIZE] = "";
char GLOBAL_PASSWORD[SIZE] = "";
char GLOBAL_DATABASE[SIZE] = "";

char *GLOBAL_DELIMITER = "[,]";
char *GLOBAL_DELIMITER_DATATYPE = "|";
char *GLOBAL_DELIMITER_DISPLAY = "\t";

char *GLOBAL_LOG_PATH = "command.log";

static void sig_handler(int signo){
    longjmp(context, 1);
}

static void catch_segv(int catch){
    struct sigaction sa;

    if (catch) {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig_handler;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGSEGV, &sa, NULL);
    } else {
        sigemptyset(&sa);
        sigaddset(&sa, SIGSEGV);
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
    }
}

static int safe_run(func_t func)
{
    catch_segv(1);

    if (setjmp(context)) {
            catch_segv(0);
            return 0;
    }

    func();
    catch_segv(0);
    return 1;
}

char* convertToCharPtr(char *str){
	int len=strlen(str);
	char* ret = malloc((len+1) * sizeof(char));
	for(int i=0; i<len; i++){
		ret[i] = str[i];
	}
	ret[len] = '\0';
	return ret;
}

char* getTimeNow(){
	time (&my_time);
	timeinfo = localtime (&my_time);

	char day[10], month[10], year[10], hour[10], minute[10], second[10];

	sprintf(day, "%d", timeinfo->tm_mday);
	if(timeinfo->tm_mday < 10) sprintf(day, "0%d", timeinfo->tm_mday);

	sprintf(month, "%d", timeinfo->tm_mon+1);
	if(timeinfo->tm_mon+1 < 10) sprintf(month, "0%d", timeinfo->tm_mon+1);

	sprintf(year, "%d", timeinfo->tm_year+1900);

	sprintf(hour, "%d", timeinfo->tm_hour);
	if(timeinfo->tm_hour < 10) sprintf(hour, "0%d", timeinfo->tm_hour);

	sprintf(minute, "%d", timeinfo->tm_min);
	if(timeinfo->tm_min < 10) sprintf(minute, "0%d", timeinfo->tm_min);

	sprintf(second, "%d", timeinfo->tm_sec);
	if(timeinfo->tm_sec < 10) sprintf(second, "0%d", timeinfo->tm_sec);

	char datetime_now[100];
	sprintf(datetime_now, "%s-%s-%s_%s:%s:%s", year, month, day, hour, minute, second);
	char* ret=convertToCharPtr(datetime_now);
	return ret;
}

char *strrev(char *str){
	  char *p1, *p2;

	  if (! str || ! *str)
			return str;
	  for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
	  {
			*p1 ^= *p2;
			*p2 ^= *p1;
			*p1 ^= *p2;
	  }
	  return str;
}

void logs(){
	time (&my_time);
	timeinfo = localtime(&my_time);

	char *timeNow = getTimeNow();
	printf("%s : %s\n", timeNow, GLOBAL_LOG_MSG);
}

void reply(int client_fd, char *message){
	send(client_fd, message, strlen(message), 0);
}

char* readClient(int client_fd){
	bzero(GLOBAL_BUFFER, SIZE);
	read(client_fd, GLOBAL_BUFFER, SIZE);
	GLOBAL_BUFFER[strcspn(GLOBAL_BUFFER, "\n")] = 0;
	return convertToCharPtr(GLOBAL_BUFFER);
}

char* uppercase(char *str){
    int i=0, len=strlen(str);
	char* ret = malloc((len+1) * sizeof(char));

    for(int i=0; i<strlen(str); i++){
        if(str[i] >= 'a' && str[i] <= 'z'){
            ret[i] = toupper(str[i]);
        }else{
            ret[i] = str[i];
        }
    }
    ret[len] = 0;
    
    return ret;
}

char* lowercase(char *str){
    int i=0, len=strlen(str);
	char* ret = malloc((len+1) * sizeof(char));
    
    while(str[i]){
        if(str[i] >= 'A' && str[i] <= 'Z')
            ret[i] = tolower(str[i]);
        else
            ret[i] = str[i];
        
        i++;
    }
    ret[len] = 0;
    
    return ret;
}

char *getStrBetween(char *str, char *PATTERN1, char *PATTERN2){
    if(PATTERN1 == NULL){
        char temp[SIZE]; sprintf(temp, "[ANISA_CHAN]%s", str);
        return getStrBetween(temp, "[ANISA_CHAN]", PATTERN2);
    }else if(PATTERN2 == NULL){
        char temp[SIZE]; sprintf(temp, "%s[ANISA_CHAN]", str);
        return getStrBetween(temp, PATTERN1, "[ANISA_CHAN]");
    }

    // printf("\tPATTERN1 : [%s]\n", PATTERN1);
    // printf("\tPATTERN2 : [%s]\n", PATTERN2);

    char *target = NULL;
    char *start, *end;

    if ( start = strstr( str, PATTERN1 ) ){
        start += strlen( PATTERN1 );
        if ( end = strstr( start, PATTERN2 ) ){
            target = ( char * )malloc( end - start + 1 );
            memcpy( target, start, end - start );
            target[end - start] = '\0';
        }
    }
    if(target == NULL){
        return NULL;
    }else{
        return target;
    }
}

char *trimStr(char *str){
    size_t len = strlen(str);

    while(isspace(str[len - 1])) --len;
    while(*str && isspace(*str)) ++str, --len;

    return strndup(str, len);
}

char* removeStrQuotes(char* str){
    if(str[0] == '\'' && str[strlen(str)-1] == '\''){
        return getStrBetween(str, "'", "'");
    }else if(str[0] == '"' && str[strlen(str)-1] == '"'){
        return getStrBetween(str, "\"", "\"");
    }else if(str[0] == '`' && str[strlen(str)-1] == '`'){
        return getStrBetween(str, "`", "`");
    }else{
        return str;
    }
}

bool isInteger(char *str){
    for(int i=0; i<strlen(str); i++){
        if(!isdigit(str[i])) return false;
    }
    return true;
}

bool file_exists(char *filename) {
  struct stat buffer;   
  return (stat(filename, &buffer) == 0);
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

struct Tabel {
    char KOLOM_NAMA[SIZE];    
    char KOLOM_TIPE[SIZE]; 
};

struct Kondisi {
    char NAMA_KOLOM[SIZE];
    char OPERATOR[SIZE];
    char KONDISI[SIZE];
};

void WRITE_INTO_TABLE(char *DATABASE, char *TABLE, char *LINE, int LINE_NUM, bool IS_DELETE){
    char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", DATABASE, TABLE);
    char file_tabel_copy[SIZE]; sprintf(file_tabel_copy, "databases/%s/%s.COPY", DATABASE, TABLE);

    if(LINE_NUM == -1){
        // WRITE AT END
        FILE* fptr_db = fopen(file_tabel, "a");
        fprintf(fptr_db, "%s\r\n", LINE);
        fclose(fptr_db);
        printf("\t\tWRITE FILE\n");
        printf("\t\tLINE :: %s\n", LINE);
    }else{
        FILE* fptr_db = fopen(file_tabel, "r");
        FILE* fptr_db_copy = fopen(file_tabel_copy, "a");

        char current_line[SIZE];
        
        int rownum=0;
        while (fgets(current_line, sizeof(current_line), fptr_db)) {
            if(rownum == LINE_NUM){
                if(!IS_DELETE){
                    fprintf(fptr_db_copy, "%s", LINE);
                }
            }else{
                fprintf(fptr_db_copy, "%s", current_line);
            }
            rownum++;
        }

        fclose(fptr_db);
        fclose(fptr_db_copy);
        
        remove(file_tabel);
        rename(file_tabel_copy, file_tabel);
    }

    printf("\t\tWRITE FILE SUCCESS\n");
}

bool AUTH(char *AUTH_STRING){
    char file_tabel[SIZE];
    sprintf(file_tabel, "databases/%s/%s", "INFORMATION_SCHEMA", "USERS");
    FILE* fptr_db = fopen(file_tabel, "r");
    char current_line[SIZE];
    fgets(current_line, sizeof(current_line), fptr_db);
    
    while(fgets(current_line, sizeof(current_line), fptr_db)){
        current_line[strlen(AUTH_STRING)] = 0;
        if(!strcmp(current_line, AUTH_STRING)){
            return true;
            break;
        }
    }

    fclose(fptr_db);
    return false;
}

void put_commandlogs(char *command){
    time (&my_time);
	timeinfo = localtime (&my_time);

	char day[10], month[10], year[10], hour[10], minute[10], second[10];

	sprintf(day, "%d", timeinfo->tm_mday);
	if(timeinfo->tm_mday < 10) sprintf(day, "0%d", timeinfo->tm_mday);

	sprintf(month, "%d", timeinfo->tm_mon+1);
	if(timeinfo->tm_mon+1 < 10) sprintf(month, "0%d", timeinfo->tm_mon+1);

	sprintf(year, "%d", timeinfo->tm_year+1900);

	sprintf(hour, "%d", timeinfo->tm_hour);
	if(timeinfo->tm_hour < 10) sprintf(hour, "0%d", timeinfo->tm_hour);

	sprintf(minute, "%d", timeinfo->tm_min);
	if(timeinfo->tm_min < 10) sprintf(minute, "0%d", timeinfo->tm_min);

	sprintf(second, "%d", timeinfo->tm_sec);
	if(timeinfo->tm_sec < 10) sprintf(second, "0%d", timeinfo->tm_sec);

    char content[SIZE];
    sprintf(content, "%s-%s-%s %s:%s:%s:%s:%s", year, month, day, hour, minute, second, GLOBAL_USERNAME, command);

    FILE* fptr_logs = fopen(GLOBAL_LOG_PATH, "a");
    fprintf(fptr_logs, "%s\r\n", content);
    fclose(fptr_logs);
}

void execute() {
    char *command = trimStr(GLOBAL_QUERY);
    char command_copy[SIZE]; sprintf(command_copy, "%s", command);
    char *cmd = uppercase(getStrBetween(command, NULL, " "));

    if(!strcmp(cmd, "CREATE")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand : %s\n", command);
        put_commandlogs(command);

        char *after_create = trimStr(getStrBetween(command, "CREATE", NULL));
        printf("\tafter_create: [%s]\n", after_create);
        char *next = uppercase(trimStr(getStrBetween(after_create, NULL, " ")));
        printf("\tNEXT: [%s]\n", uppercase(trimStr(getStrBetween(after_create, NULL, " "))));

        if(!strcmp(next, "USER")) {
            if(!GLOBAL_IS_ROOT){
                sprintf(GLOBAL_REPLY_MSG, "%sERROR:: You don't have permission to create users\r\n", GLOBAL_REPLY_MSG);
                return;
            }
            // Information
            char *username = "";
            char *password = "";

            username = trimStr(getStrBetween(command, "USER ", " "));
            next = trimStr(getStrBetween(trimStr(getStrBetween(command, username, NULL)), NULL, " "));

            if(!strcmp(next, "IDENTIFIED")) {
                next = trimStr(getStrBetween(trimStr(getStrBetween(command, next, NULL)), NULL, " "));
                
                if(!strcmp(next, "BY")) {
                    password = trimStr(getStrBetween(trimStr(getStrBetween(command, next, NULL)), NULL, ";"));
                }else{
                    // return error;
                }
            }else{
                // return error
            }

            printf("\tUsername : [%s]\n", username);
            printf("\tPassword : [%s]\n", password);

            // Start the thing
            // INSERT INTO USERS ('%s', '%s', 'NULL');
            sprintf(GLOBAL_QUERY, "INSERT INTO USERS ('%s', '%s', '');", username, password);
            execute();
            sprintf(GLOBAL_REPLY_MSG, "%sCREATE USER SUCCESS\r\n", GLOBAL_REPLY_MSG);
        }
        else if(!strcmp(next, "DATABASE")) {
            if(!GLOBAL_IS_ROOT){
                sprintf(GLOBAL_REPLY_MSG, "%sERROR:: You don't have permission to create database\r\n", GLOBAL_REPLY_MSG);
                return;
            }

            // Information
            char *nama_database = "";

            nama_database = uppercase(trimStr(getStrBetween(command, "DATABASE ", ";")));
            printf("\tnama_database: [%s]\n", nama_database);

            // CHECK IF DATABASE EXIST
            char path_db[SIZE];
            sprintf(path_db, "databases/%s", nama_database);
            DIR* dir_exist = opendir(path_db);

            if(dir_exist){
                // DATABASE EXIST
                sprintf(GLOBAL_REPLY_MSG, "%sDATABASE ALREADY EXIST\r\n", GLOBAL_REPLY_MSG);
            }else{
                mkdir(path_db, "0777");
                sprintf(GLOBAL_REPLY_MSG, "%sCREATE DATABASE SUCCESS\r\n", GLOBAL_REPLY_MSG);
            }
        }
        else if(!strcmp(next, "TABLE")) {
            // Information
            printf("\tMASUK TABLE\n");
            char *nama_tabel = uppercase(trimStr(getStrBetween(command, "TABLE ", " ")));
            char *tabel_str = trimStr(getStrBetween(command, "(", ")"));

            printf("\tnama_table: [%s]\n", nama_tabel);
            printf("\ttabel_str: [%s]\n", tabel_str);

            // Sekarang baca entitas dan tipe datanya
            struct Tabel tabel_list[100];
            int index = 0;
            char *tok = strtok(tabel_str, ",");
            while(tok != NULL) {
                tok = trimStr(tok);
                printf("\t\tKolom : %s\n", tok);
                char *kolom_nama = getStrBetween(tok, NULL, " ");
                char *kolom_tipe = getStrBetween(tok, " ", NULL);
                printf("\t\t\tkolom_nama : [%s]\n", kolom_nama);
                printf("\t\t\tkolom_tipe : [%s]\n", kolom_tipe);
                sprintf(tabel_list[index].KOLOM_NAMA, "%s", uppercase(kolom_nama));
                sprintf(tabel_list[index].KOLOM_TIPE, "%s", uppercase(kolom_tipe));
                tok = strtok(NULL, ",");
                index++;
            }

            // Lalu Buat Tabelnya
            // SEBELUMNYA, CHECK DULU TABELNYA UDAH ADA APA BELUM
            char path_tabel[SIZE];
            sprintf(path_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
            if(file_exists(path_tabel)){
                // TABLE EXIST
                sprintf(GLOBAL_REPLY_MSG, "%sTABLE ALREADY EXIST\r\n", GLOBAL_REPLY_MSG);
            }else{
                // LALU BUAT LINE HEADERNYA
                char LINE_TO_WRITE[SIZE]; sprintf(LINE_TO_WRITE, "");
                for(int i=0; i<index; i++){
                    if(i != 0){
                        sprintf(LINE_TO_WRITE, "%s%s%s%s%s", LINE_TO_WRITE, GLOBAL_DELIMITER, tabel_list[i].KOLOM_NAMA, GLOBAL_DELIMITER_DATATYPE, tabel_list[i].KOLOM_TIPE);
                    }else{
                        sprintf(LINE_TO_WRITE, "%s%s%s", tabel_list[i].KOLOM_NAMA, GLOBAL_DELIMITER_DATATYPE, tabel_list[i].KOLOM_TIPE);
                    }
                }

                printf("\t\tLINE TO WRITE = [%s]\n", LINE_TO_WRITE);

                FILE* fptr_tabel = fopen(path_tabel, "w");
                fprintf(fptr_tabel, "%s\r\n", LINE_TO_WRITE);
                fclose(fptr_tabel);
            }
        }
    }
    else if(!strcmp(cmd, "DROP")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand : %s\n", command);
        put_commandlogs(command);

        char *next = uppercase(getStrBetween(command, "DROP ", " "));

        if(!strcmp(next, "DATABASE")) {
            // Information
            char *nama_database = uppercase(trimStr(getStrBetween(command, "DATABASE ", ";")));
            printf("\tnama_database : [%s]\n", nama_database);

            // JANGAN LUPA BUAT CHECK PERMISSION
            char temp_auth_string[SIZE];
            sprintf(temp_auth_string, "%s%s%s%s%s", GLOBAL_USERNAME, GLOBAL_DELIMITER, GLOBAL_PASSWORD, GLOBAL_DELIMITER, nama_database);

            if(!AUTH(temp_auth_string)){
                // MAAF ANDA TIDAK PUNTA IJIN UNTUK DROP DB INI
                sprintf(GLOBAL_REPLY_MSG, "%sERROR:: You don't have permission to drop [%s]\r\n", GLOBAL_REPLY_MSG, nama_database);
                return;
            }

            // CHECK IF DATABASE EXIST
            char path_db[SIZE];
            sprintf(path_db, "databases/%s", nama_database);
            DIR* dir_exist = opendir(path_db);

            if(dir_exist){
                // DATABASE EXIST, THEN DROP
                rmrf(path_db);
                sprintf(GLOBAL_REPLY_MSG, "%sDROP DATABASE [%s] SUCCESS\r\n", GLOBAL_REPLY_MSG, nama_database);
            }else{
                sprintf(GLOBAL_REPLY_MSG, "%sDATABASE NOT EXIST\r\n", GLOBAL_REPLY_MSG);
            }
        }
        else if(!strcmp(next, "TABLE")) {
            // Information
            char *nama_tabel = uppercase(trimStr(getStrBetween(command, "TABLE ", ";")));
            printf("\tnama_tabel : [%s]\n", nama_tabel);

            // CHECK IF TABLE EXIST
            char path_tabel[SIZE];
            sprintf(path_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
            if(file_exists(path_tabel)){
                // TABLE EXIST, THEN DROP
                sprintf(GLOBAL_REPLY_MSG, "%sDROP TABLE [%s] SUCCESS!\r\n", GLOBAL_REPLY_MSG, nama_tabel);
                remove(path_tabel);
            }else{
                // TABLE DOES NOT EXIST, THEN reply error
                sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO DROP TABLE [%s] : TABLE DOES NOT EXIST\r\n", GLOBAL_REPLY_MSG, nama_tabel);
            }
        }
        else if(!strcmp(next, "COLUMN")) {
            char *nama_kolom = "";
            char *nama_tabel = "";

            nama_kolom = uppercase(trimStr(getStrBetween(command, "COLUMN ", " ")));
            nama_tabel = uppercase(trimStr(getStrBetween(command, "FROM ", ";")));
            printf("\tnama_kolom : [%s]\n", nama_kolom);
            printf("\tnama_tabel : [%s]\n", nama_tabel);

            if(strlen(nama_kolom) == 0 || strlen(nama_tabel) == 0){
                // return error
                sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO DROP COLUMN [%s] FROM TABLE [%s]\r\n", GLOBAL_REPLY_MSG, nama_kolom, nama_tabel);
            }

            // THEN CHECK IF TABLE EXIST
            // CHECK IF TABLE EXIST
            char path_tabel[SIZE];
            sprintf(path_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
            if(!file_exists(path_tabel)){
                // TABLE DOES NOT EXIST, THEN reply error
                sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO DROP COLUMN [%s] FROM TABLE [%s] : TABLE DOES NOT EXIST\r\n", GLOBAL_REPLY_MSG, nama_kolom, nama_tabel);
                return;
            }

            // TABELNYA ADA LALU LANJUT CEK KOLOMNYA
            // SEKARANG BACA HEADER TABELNYA
            char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
            FILE* fptr_db = fopen(file_tabel, "r");
            char header_line[SIZE]; fgets(header_line, sizeof(header_line), fptr_db);
            fclose(fptr_db);

            // LALU DI PECAH MASING-MASING HEADER MASUKIN KE ARRAY
            int index_header = 0;
            int index_header_tofind = -1;

            char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
            while(tok_header != NULL) {
                char *value_name = trimStr(getStrBetween(tok_header, NULL, "|"));

                if(!strcmp(value_name, nama_kolom)){
                    index_header_tofind = index_header;
                    break;
                }
                
                printf("\t\tHEADER_NAME : %s\n", value_name);
                tok_header = strtok(NULL, GLOBAL_DELIMITER);
                index_header++;
            }

            // VALIDASI APAKAH TABELNYA DITEMUKAN???
            if(index_header_tofind == -1){
                // COLUMN DOES NOT EXIST, THEN reply error
                sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO DROP COLUMN [%s] : COLUMN DOES NOT EXIST\r\n", GLOBAL_REPLY_MSG, nama_tabel);
                return;
            }

            // LALU TULIS ULANG TABELNYA TAPI PADA KOLOM TERTENTU TIDAK DILIHATIN
            char file_tabel_copy[SIZE]; sprintf(file_tabel_copy, "databases/%s/%s.COPY", GLOBAL_DATABASE, nama_tabel);

            fptr_db = fopen(file_tabel, "r");
            FILE* fptr_db_copy = fopen(file_tabel_copy, "a");

            char current_line[SIZE]; char to_write[SIZE];
            int rownum = 0;
            while (fgets(current_line, sizeof(current_line), fptr_db)) {
                int colnum=0;
                // STRTOK
                char *CURR_DELIMITER = GLOBAL_DELIMITER;
                // if(rownum == 0){
                //     CURR_DELIMITER = GLOBAL_DELIMITER_DATATYPE;
                // }else{
                //     CURR_DELIMITER = GLOBAL_DELIMITER;
                // }

                char *tok_kolom = strtok(current_line, CURR_DELIMITER);
                while(tok_kolom != NULL) {
                    char *kolom = removeStrQuotes(trimStr(tok_kolom));
                    printf("\t\tkolom : %s\n", kolom);

                    if (colnum != index_header_tofind){
                        if(colnum == 0 || (colnum == 1 && index_header_tofind==0)){
                            sprintf(to_write, "%s", kolom);
                        }else{
                            sprintf(to_write, "%s%s%s", to_write, CURR_DELIMITER, kolom);
                        }
                    }

                    tok_kolom = strtok(NULL, CURR_DELIMITER);
                    colnum++;
                }
                fprintf(fptr_db_copy, "%s\r\n", to_write);
                sprintf(to_write, "");
                rownum++;
            }

            fclose(fptr_db);
            fclose(fptr_db_copy);
            
            remove(file_tabel);
            rename(file_tabel_copy, file_tabel);

            sprintf(GLOBAL_REPLY_MSG, "%sDROP COLUMN [%s] FROM TABLE [%s] SUCCESS\r\n", GLOBAL_REPLY_MSG, nama_kolom, nama_tabel);
        }
    }
    else if(!strcmp(cmd, "INSERT")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("Command: [%s]\n", command);
        put_commandlogs(command);
        
        // INFORMATION
        char *nama_tabel = "";
        char *tabel_str = "";
        char kolom_set[100][SIZE]; int index_set = 0;
        char kolom_value[100][SIZE]; int index_value = 0;
        for(int i=0; i<100; i++){
            sprintf(kolom_value[i], " ");
        }

        nama_tabel = trimStr(getStrBetween(command, "INTO ", " "));
        char *after_tabel = trimStr(getStrBetween(command, nama_tabel, NULL));
        char is_set[SIZE]; sprintf(is_set, "%s", after_tabel);
        is_set[strlen("SET")] = '\0';

        nama_tabel = uppercase(nama_tabel);
        printf("\tnama_table: [%s]\n", nama_tabel);
        printf("\tafter_tabel: [%s]\n", after_tabel);

        char *tabel_value = "";

        // Lalu di cek apakah dia SET
        if(!strcmp(is_set, "SET")){
            // SET (satu, dua, tiga) VALUES ('v1', 2, 'v3')
            char *tabel_kolom = trimStr(getStrBetween(command, "(", ")"));

            char *tok = strtok(tabel_kolom, ",");
            while(tok != NULL) {
                char *kolom = removeStrQuotes(trimStr(tok));
                sprintf(kolom_set[index_set], "%s", uppercase(kolom));
                printf("\t\tkolom : %s\n", kolom_set[index_set]);
                tok = strtok(NULL, ",");
                index_set++;
            }
            tabel_value = trimStr(getStrBetween(command, "VALUES (", ")"));
        }else{
            // Yang ini normal tidak ada di soal shift
            tabel_value = trimStr(getStrBetween(command, "(", ")"));
        }

        printf("\t\ttabel_value = %s\n", tabel_value);

        char *tok = strtok(tabel_value, ",");
        while(tok != NULL) {
            char *value = removeStrQuotes(trimStr(tok));
            if(!strcmp(value, "")){
                value = " ";
            }
            sprintf(kolom_value[index_value], "%s", value);
            printf("\t\tvalue : %s\n", kolom_value[index_value]);
            tok = strtok(NULL, ",");
            index_value++;
        }
        
        // SEKARANG BACA HEADER TABELNYA
        char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
        FILE* fptr_db = fopen(file_tabel, "r");
        char header_line[SIZE]; fgets(header_line, sizeof(header_line), fptr_db);

        // LALU DI PECAH MASING-MASING HEADER MASUKIN KE ARRAY
        char header_name_set[100][SIZE]; int index_header = 0;
        char header_name_type[100][SIZE];

        char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
        while(tok_header != NULL) {
            char *value_name = trimStr(getStrBetween(tok_header, NULL, "|"));
            char *value_type = trimStr(getStrBetween(tok_header, "|", NULL));
            
            sprintf(header_name_set[index_header], "%s", value_name);
            sprintf(header_name_type[index_header], "%s", value_type);
            
            printf("\t\tHEADER_NAME : %s\n", header_name_set[index_header]);
            printf("\t\tHEADER_TYPE : %s\n", header_name_type[index_header]);

            // KALOK NGGAK ISI SET, MAKA KOLOM_SET NYA DIAMBIL DARI DATABASE
            if(!!strcmp(is_set, "SET")){
                sprintf(kolom_set[index_header], "%s", header_name_set[index_header]);
            }

            tok_header = strtok(NULL, GLOBAL_DELIMITER);
            index_header++;
        }

        char to_write[SIZE];
        sprintf(to_write, "");

        int index_kolom_now = 0;

        for(int i=0; i<index_header; i++){
            if(!strcmp(header_name_set[i], kolom_set[index_kolom_now])){
                // SEKARANG DICEK APAKAH DATA YANG DI SET TYPE NYA UDAH SAMA APA BELOOM
                if(!strcmp(header_name_type[i], "INT")){
                    if(!isInteger(kolom_value[index_kolom_now])){
                        // return error
                        sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO INSERT DATA, DATATYPE NOT SAME AT COLUMN [%s] (%s) TRYING TO FILL WITH [%s]\r\n", GLOBAL_REPLY_MSG, header_name_set[i], header_name_type[i], kolom_value[index_kolom_now]);
                        return;
                    }else{
                        int temp_value = atoi(kolom_value[index_kolom_now]);
                        sprintf(kolom_value[index_kolom_now], "%d", temp_value);
                    }
                }

                if(i!=0){
                    if(!strcmp(trimStr(kolom_value[index_kolom_now]), "")){
                        sprintf(to_write, "%s%s%s", to_write, GLOBAL_DELIMITER, " ");
                    }else{
                        sprintf(to_write, "%s%s%s", to_write, GLOBAL_DELIMITER, kolom_value[index_kolom_now]);
                    }
                }else{
                    if(!strcmp(trimStr(kolom_value[index_kolom_now]), "")){
                        sprintf(to_write, "%s", " ");
                    }else{
                        sprintf(to_write, "%s", kolom_value[index_kolom_now]);
                    }
                }
                index_kolom_now++;
            }else{
                if(i!=0){
                    sprintf(to_write, "%s%s%s", to_write, GLOBAL_DELIMITER, " ");
                }else{
                    sprintf(to_write, "%s", " ");
                }
            }
        }

        printf("\t\tto_write : %s\n", to_write);
        WRITE_INTO_TABLE(GLOBAL_DATABASE, nama_tabel, to_write, -1, false);
        sprintf(GLOBAL_REPLY_MSG, "%sINSERT TABLE SUCCESS\r\n", GLOBAL_REPLY_MSG);
    }
    else if(!strcmp(cmd, "UPDATE")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand: [%s]\n", command);
        put_commandlogs(command);

        // Information
        char *nama_tabel = "";
        char *kolom_set = "";
        char *value_set = "";
        struct Kondisi *kondisi_update = malloc(sizeof(struct Kondisi));
        
        char kolom_list[100][SIZE]; int _index = 0;
        char value_list[100][SIZE];

        nama_tabel = uppercase(trimStr(getStrBetween(command, "UPDATE", "SET")));
        if(strstr(command, "WHERE")){
            kolom_set = trimStr(getStrBetween(command, "SET", "WHERE"));
            // Baca kondisinya
            char *after_where = trimStr(getStrBetween(command, "WHERE", ";"));
            if(strstr(after_where, "!=")){
                sprintf(kondisi_update->OPERATOR, "%s", "!=");
            }else{
                sprintf(kondisi_update->OPERATOR, "%s", "=");
            }
            sprintf(kondisi_update->NAMA_KOLOM, "%s", uppercase(trimStr(getStrBetween(after_where, NULL, kondisi_update->OPERATOR))));
            sprintf(kondisi_update->KONDISI, "%s", uppercase(removeStrQuotes(trimStr(getStrBetween(after_where, kondisi_update->OPERATOR, NULL)))));
        }else{
            sprintf(kondisi_update->OPERATOR, "%s", "TO_ALL");
            kolom_set = trimStr(getStrBetween(command, "SET", ";"));
        }

        printf("\tnama_tabel : %s\n", nama_tabel);
        printf("\tkolom_set : %s\n", kolom_set);

        char *tok = strtok(kolom_set, ",");
        while(tok != NULL) {
            char *kolom_nama = uppercase(getStrBetween(trimStr(tok), NULL, "="));
            char *kolom_nilai = removeStrQuotes(getStrBetween(trimStr(tok), "=", NULL));

            sprintf(kolom_list[_index], "%s", kolom_nama);
            sprintf(value_list[_index], "%s", kolom_nilai);

            printf("\t\tKolom_nama : [%s]\n", kolom_list[_index]);
            printf("\t\tKolom_nilai : [%s]\n", value_list[_index]);
                        
            tok = strtok(NULL, ",");
            _index++;
        }

        printf("\tkondisi_update.NAMA_KOLOM : [%s]\n", kondisi_update->NAMA_KOLOM);
        printf("\tkondisi_update.OPERATOR : [%s]\n", kondisi_update->OPERATOR);
        printf("\tkondisi_update.KONDISI : [%s]\n", kondisi_update->KONDISI);

        // JANGAN LUPA CEK TABELNYA ADA APA ENGGAK

        // MULAI UPDATE
        // SEKARANG BACA HEADER TABELNYA
        char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
        if(!file_exists(file_tabel)){
            sprintf(GLOBAL_REPLY_MSG, "%sERROR: TABLE [%s] in DATABASE [%s]\r\n", GLOBAL_REPLY_MSG, nama_tabel, GLOBAL_DATABASE);
            return;
        }

        char file_tabel_copy[SIZE]; sprintf(file_tabel_copy, "databases/%s/%s.COPY", GLOBAL_DATABASE, nama_tabel);
        FILE* fptr_db = fopen(file_tabel, "r");
        FILE* fptr_db_copy = fopen(file_tabel_copy, "w");

        char header_line[SIZE]; fgets(header_line, sizeof(header_line), fptr_db);
        fclose(fptr_db);
        fptr_db = fopen(file_tabel, "r");
        
        // LALU DI PECAH MASING-MASING HEADER MASUKIN KE ARRAY
        char header_name_set[100][SIZE]; int index_header = 0;
        char header_name_type[100][SIZE];

        int replaces_index[100]; memset(replaces_index, -1, sizeof(replaces_index));
        int kondisi_update_index = -1;

        char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
        while(tok_header != NULL) {
            char *value_name = uppercase(trimStr(getStrBetween(tok_header, NULL, "|")));
            char *value_type = trimStr(getStrBetween(tok_header, "|", NULL));
            
            for(int i=0; i<_index; i++){
                if(!strcmp(value_name, kolom_list[i])){
                    replaces_index[index_header] = i;
                    break;
                }
            }

            if(!strcmp(value_name, kondisi_update->NAMA_KOLOM)){
                kondisi_update_index = index_header;
            }

            sprintf(header_name_set[index_header], "%s", value_name);
            sprintf(header_name_type[index_header], "%s", value_type);
            
            printf("\t\tHEADER_NAME : %s\n", header_name_set[index_header]);
            printf("\t\tHEADER_TYPE : %s\n", header_name_type[index_header]);

            tok_header = strtok(NULL, GLOBAL_DELIMITER);
            index_header++;
        }

        for(int i=0; i<index_header; i++){
            printf("\t\t\treplaces_index[%d] = %d\n", i, replaces_index[i]);
        }

        char to_write[SIZE];
        sprintf(to_write, "");

        int index_row_now = 0;
        char current_line[SIZE];
        while(fgets(current_line, sizeof(current_line), fptr_db)){
            
            current_line[strcspn(current_line, "\r\n")] = 0;
            char current_line_copy[SIZE];
            sprintf(current_line_copy, "%s", current_line);
            printf("\t\tcurrent line : [%s]\n", current_line);
            if(index_row_now == 0){
                fprintf(fptr_db_copy, "%s\r\n", current_line);
                index_row_now++;
                continue;
            }

            char temp_kolom_value[100][SIZE];
            int index_temp = 0;

            char *tok = strtok(current_line, GLOBAL_DELIMITER);
            while(tok != NULL) {
                char *kolom_nama = trimStr(tok);
                sprintf(temp_kolom_value[index_temp], "%s", kolom_nama);

                printf("\t\t\tNilai kolom : [%s]\n", temp_kolom_value[index_temp]);                        
                tok = strtok(NULL, GLOBAL_DELIMITER);
                index_temp++;
            }

            // Lalu isi to_writenya
            int index_kolom_now = 0;
            for(int i=0; i<index_header; i++){
                char value_write[SIZE]; sprintf(value_write, "");

                if(replaces_index[i] != -1){
                    // SEKARANG DICEK APAKAH DATA YANG DI SET TYPE NYA UDAH SAMA APA BELOOM
                    if(!strcmp(header_name_type[i], "INT")){
                        if(!isInteger(value_list[replaces_index[i]])){
                            // return error
                            sprintf(GLOBAL_REPLY_MSG, "%sFAILED TO INSERT DATA, DATATYPE NOT SAME AT COLUMN [%s] (%s) TRYING TO FILL WITH [%s]\r\n", GLOBAL_REPLY_MSG, header_name_set[i], header_name_type[i], value_list[index_kolom_now]);
                            return;
                        }else{
                            int temp_value = atoi(value_list[replaces_index[i]]);
                            sprintf(value_list[replaces_index[i]], "%d", temp_value);
                        }
                    }
                    if(!strcmp(trimStr(value_list[replaces_index[i]]), "")){
                        sprintf(value_write, " ");
                    }else{
                        sprintf(value_write, "%s", value_list[replaces_index[i]]);
                    }
                    index_kolom_now++;
                }
                else {
                    if(!strcmp(trimStr(temp_kolom_value[i]), "")){
                        sprintf(value_write, " ");
                    }else{
                        sprintf(value_write, "%s", temp_kolom_value[i]);
                    }
                }

                printf("\t\t\tvalue_write : %s\n", value_write);

                if(i!=0){
                    sprintf(to_write, "%s%s%s", to_write, GLOBAL_DELIMITER, value_write);
                }else{
                    sprintf(to_write, "%s", value_write);
                }
            }

            printf("\t\t\tto_write : %s\n", to_write);

            // SEKARANG CEK APAKAH WHERE STATEMENTNYA BENER
            if((!strcmp(kondisi_update->OPERATOR, "=") && !strcmp(temp_kolom_value[kondisi_update_index], kondisi_update->KONDISI)) || (!strcmp(kondisi_update->OPERATOR, "!=") && !!strcmp(temp_kolom_value[kondisi_update_index], kondisi_update->KONDISI)) || !strcmp(kondisi_update->OPERATOR, "TO_ALL")){
                // UPDATE KOLOM
                fprintf(fptr_db_copy, "%s\r\n", to_write);
            }else{
                fprintf(fptr_db_copy, "%s\r\n", current_line_copy);
            }
            sprintf(current_line, "");
            index_row_now++;
        }

        fclose(fptr_db);
        fclose(fptr_db_copy);
        remove(file_tabel);
        rename(file_tabel_copy, file_tabel);
    }
    else if(!strcmp(cmd, "DELETE")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand: [%s]\n", command);
        put_commandlogs(command);

        // Information
        char *nama_tabel = "";
        struct Kondisi *kondisi_update = malloc(sizeof(struct Kondisi));

        if(strstr(command, "WHERE")){
            nama_tabel = uppercase(trimStr(getStrBetween(command, "FROM", "WHERE")));
            // Baca kondisinya
            char *after_where = trimStr(getStrBetween(command, "WHERE", ";"));
            if(strstr(after_where, "!=")){
                sprintf(kondisi_update->OPERATOR, "%s", "!=");
            }else{
                sprintf(kondisi_update->OPERATOR, "%s", "=");
            }
            sprintf(kondisi_update->NAMA_KOLOM, "%s", uppercase(trimStr(getStrBetween(after_where, NULL, kondisi_update->OPERATOR))));
            sprintf(kondisi_update->KONDISI, "%s", removeStrQuotes(trimStr(getStrBetween(after_where, kondisi_update->OPERATOR, NULL))));
        }else{
            sprintf(kondisi_update->OPERATOR, "TO_ALL");
            nama_tabel = uppercase(trimStr(getStrBetween(command, "FROM", ";")));
        }

        printf("\tnama_tabel : [%s]\n", nama_tabel);
        printf("\tkondisi_update.NAMA_KOLOM : [%s]\n", kondisi_update->NAMA_KOLOM);
        printf("\tkondisi_update.OPERATOR : [%s]\n", kondisi_update->OPERATOR);
        printf("\tkondisi_update.KONDISI : [%s]\n", kondisi_update->KONDISI);

        // CHECK NAMA TABELNYA ADA APA GAK?
        char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
        if(!file_exists(file_tabel)){
            sprintf(GLOBAL_REPLY_MSG, "%sERROR: TABLE [%s] in DATABASE [%s]\r\n", GLOBAL_REPLY_MSG, nama_tabel, GLOBAL_DATABASE);
            return;
        }

        char file_tabel_copy[SIZE]; sprintf(file_tabel_copy, "databases/%s/%s.COPY", GLOBAL_DATABASE, nama_tabel);
        FILE* fptr_db = fopen(file_tabel, "r");
        FILE* fptr_db_copy = fopen(file_tabel_copy, "w");

        char header_line[SIZE]; fgets(header_line, sizeof(header_line), fptr_db);
        char header_line_copy[SIZE]; sprintf(header_line_copy, "%s", header_line);
        fclose(fptr_db);
        fptr_db = fopen(file_tabel, "r");

        // LALU DI PECAH MASING-MASING HEADER MASUKIN KE ARRAY
        char header_name_set[100][SIZE]; int index_header = 0;
        char header_name_type[100][SIZE];

        int kondisi_update_index = -1;

        char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
        while(tok_header != NULL) {
            char *value_name = uppercase(trimStr(getStrBetween(tok_header, NULL, "|")));
            char *value_type = trimStr(getStrBetween(tok_header, "|", NULL));
            
            if(!strcmp(value_name, kondisi_update->NAMA_KOLOM)){
                kondisi_update_index = index_header;
            }

            sprintf(header_name_set[index_header], "%s", value_name);
            sprintf(header_name_type[index_header], "%s", value_type);
            
            printf("\t\tHEADER_NAME : %s\n", header_name_set[index_header]);
            printf("\t\tHEADER_TYPE : %s\n", header_name_type[index_header]);

            tok_header = strtok(NULL, GLOBAL_DELIMITER);
            index_header++;
        }

        int index_row_now = 0;
        char current_line[SIZE];
        while(fgets(current_line, sizeof(current_line), fptr_db)){
            char to_write[SIZE];
            sprintf(to_write, "");        
            current_line[strcspn(current_line, "\r\n")] = 0;
            char current_line_copy[SIZE];
            sprintf(current_line_copy, "%s", current_line);
            printf("\t\tcurrent line : [%s]\n", current_line);
            if(index_row_now == 0){
                fprintf(fptr_db_copy, "%s\r\n", current_line);
                index_row_now++;
                continue;
            }

            char temp_kolom_value[100][SIZE];
            int index_temp = 0;

            char *tok = strtok(current_line, GLOBAL_DELIMITER);
            while(tok != NULL) {
                char *kolom_nama = trimStr(tok);
                sprintf(temp_kolom_value[index_temp], "%s", kolom_nama);

                printf("\t\t\tNilai kolom : [%s]\n", temp_kolom_value[index_temp]);                        
                tok = strtok(NULL, GLOBAL_DELIMITER);
                index_temp++;
            }

            printf("\t\t\tto_write : %s\n", to_write);

            // SEKARANG CEK APAKAH WHERE STATEMENTNYA BENER
            if((!strcmp(kondisi_update->OPERATOR, "=") && !strcmp(temp_kolom_value[kondisi_update_index], kondisi_update->KONDISI)) || (!strcmp(kondisi_update->OPERATOR, "!=") && !!strcmp(temp_kolom_value[kondisi_update_index], kondisi_update->KONDISI)) || !strcmp(kondisi_update->OPERATOR, "TO_ALL")){
                // JANGAN DI WRITE
            }else{
                fprintf(fptr_db_copy, "%s\r\n", current_line_copy);
            }
            sprintf(current_line, "");
        }

        fclose(fptr_db);
        fclose(fptr_db_copy);
        remove(file_tabel);
        rename(file_tabel_copy, file_tabel);
    }
    else if(!strcmp(cmd, "SELECT")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand: [%s]\n", command);
        put_commandlogs(command);

        // Information
        char *nama_tabel = "";
        struct Kondisi *kondisi_update = malloc(sizeof(struct Kondisi));

        if(strstr(command, "WHERE")){
            nama_tabel = uppercase(trimStr(getStrBetween(command, "FROM", "WHERE")));
            // Baca kondisinya
            char *after_where = trimStr(getStrBetween(command, "WHERE", ";"));
            if(strstr(after_where, "!=")){
                sprintf(kondisi_update->OPERATOR, "%s", "!=");
            }else{
                sprintf(kondisi_update->OPERATOR, "%s", "=");
            }
            sprintf(kondisi_update->NAMA_KOLOM, "%s", uppercase(trimStr(getStrBetween(after_where, NULL, kondisi_update->OPERATOR))));
            sprintf(kondisi_update->KONDISI, "%s", removeStrQuotes(trimStr(getStrBetween(after_where, kondisi_update->OPERATOR, NULL))));
        }else{
            sprintf(kondisi_update->OPERATOR, "TO_ALL");
            nama_tabel = uppercase(trimStr(getStrBetween(command, "FROM", ";")));
        }

        printf("\tnama_tabel : [%s]\n", nama_tabel);
        printf("\tkondisi_update.NAMA_KOLOM : [%s]\n", kondisi_update->NAMA_KOLOM);
        printf("\tkondisi_update.OPERATOR : [%s]\n", kondisi_update->OPERATOR);
        printf("\tkondisi_update.KONDISI : [%s]\n", kondisi_update->KONDISI);

        char *kolom_set = trimStr(getStrBetween(command, "SELECT", "FROM"));
        char kolom_list[100][SIZE]; int index_kolom = 0;
        
        char *tok = strtok(kolom_set, ",");
        while(tok != NULL) {
            char *kolom_nama = uppercase(trimStr(tok));
            sprintf(kolom_list[index_kolom], "%s", kolom_nama);

            printf("\t\tkolom_list[%d] : [%s]\n", index_kolom, kolom_list[index_kolom]);                        
            tok = strtok(NULL, ",");
            index_kolom++;
        }

        printf("\tnama_tabel: [%s]\n", nama_tabel);

        // CHECK NAMA TABELNYA ADA APA GAK?
        char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, nama_tabel);
        if(!file_exists(file_tabel)){
            sprintf(GLOBAL_REPLY_MSG, "%sERROR: TABLE [%s] in DATABASE [%s]\r\n", GLOBAL_REPLY_MSG, nama_tabel, GLOBAL_DATABASE);
            return;
        }
        FILE* fptr_db = fopen(file_tabel, "r");
        char header_line[SIZE]; fgets(header_line, sizeof(header_line), fptr_db);
        fclose(fptr_db);

        // LALU DI PECAH MASING-MASING HEADER MASUKIN KE ARRAY
        char header_name_set[100][SIZE]; int index_header = 0;
        char header_name_type[100][SIZE];

        // SEKARANG CARI KOLOM YANG AKAN DIDISPLAY
        // BACA HEADERNYA
        bool index_to_display[100];
        memset(index_to_display, false, sizeof(index_to_display));

        int kondisi_where_index = -1;

        char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
        while(tok_header != NULL) {
            char *value_name = uppercase(trimStr(getStrBetween(tok_header, NULL, "|")));
            char *value_type = trimStr(getStrBetween(tok_header, "|", NULL));
            
            if(!strcmp(value_name, kondisi_update->NAMA_KOLOM)){
                kondisi_where_index = index_header;
            }

            // Check index_to_displaynya yg mana aja~
            for(int i=0; i<index_kolom; i++){
                if(!strcmp(value_name, kolom_list[i])){
                    index_to_display[index_header] = true;
                    break;
                }
            }

            sprintf(header_name_set[index_header], "%s", value_name);
            sprintf(header_name_type[index_header], "%s", value_type);
            
            printf("\t\tHEADER_NAME : %s\n", header_name_set[index_header]);
            printf("\t\tHEADER_TYPE : %s\n", header_name_type[index_header]);

            tok_header = strtok(NULL, GLOBAL_DELIMITER);
            index_header++;
        }

        int index_row_now = 0;
        char current_line[SIZE];
        
        fptr_db = fopen(file_tabel, "r");
        while(fgets(current_line, sizeof(current_line), fptr_db)){
            current_line[strcspn(current_line, "\r\n")] = 0;
            char current_line_copy[SIZE];
            sprintf(current_line_copy, "%s", current_line);
            printf("\t\tcurrent line : [%s]\n", current_line);

            printf("\t\tindex_row_now : %d\n", index_row_now);
            if(index_row_now == 0){
                // PRINT HEADERNYA
                char *tok = strtok(current_line, GLOBAL_DELIMITER);
                int index_print_header = 0;
                while(tok != NULL) {
                    char *kolom_nama = trimStr(getStrBetween(tok, NULL, "|"));
                    if(index_to_display[index_print_header] || !strcmp(kolom_list[0], "*")){
                        if(index_print_header == 0){
                            sprintf(GLOBAL_REPLY_MSG, "%s%s", GLOBAL_REPLY_MSG, kolom_nama);
                        }else{
                            sprintf(GLOBAL_REPLY_MSG, "%s%s%s", GLOBAL_REPLY_MSG, GLOBAL_DELIMITER_DISPLAY, kolom_nama);
                        }
                    }

                    tok = strtok(NULL, GLOBAL_DELIMITER);
                    index_print_header++;
                }
                index_row_now++;
                sprintf(GLOBAL_REPLY_MSG, "%s\r\n", GLOBAL_REPLY_MSG);
                continue;
            }

            char temp_kolom_value[100][SIZE];
            int index_temp = 0;

            char *tok = strtok(current_line, GLOBAL_DELIMITER);
            while(tok != NULL) {
                char *kolom_nama = trimStr(tok);
                sprintf(temp_kolom_value[index_temp], "%s", kolom_nama);
                printf("\t\t\tNilai kolom : [%s]\n", temp_kolom_value[index_temp]);                        
                tok = strtok(NULL, GLOBAL_DELIMITER);
                index_temp++;
            }

            // SEKARANG CEK APAKAH WHERE STATEMENTNYA BENER
            if((!strcmp(kondisi_update->OPERATOR, "=") && !strcmp(temp_kolom_value[kondisi_where_index], kondisi_update->KONDISI)) || (!strcmp(kondisi_update->OPERATOR, "!=") && !!strcmp(temp_kolom_value[kondisi_where_index], kondisi_update->KONDISI)) || !strcmp(kondisi_update->OPERATOR, "TO_ALL")){
                printf("\t\t\tSTATEMEN BERNILAI TRUE\n");
                for(int i=0; i<index_temp; i++){
                    if(index_to_display[i] || !strcmp(kolom_list[0], "*")){
                        if(i == 0){
                            sprintf(GLOBAL_REPLY_MSG, "%s%s", GLOBAL_REPLY_MSG, temp_kolom_value[i]);
                        }else{
                            sprintf(GLOBAL_REPLY_MSG, "%s%s%s", GLOBAL_REPLY_MSG, GLOBAL_DELIMITER_DISPLAY, temp_kolom_value[i]);
                        }
                    }
                }
                sprintf(GLOBAL_REPLY_MSG, "%s\r\n", GLOBAL_REPLY_MSG);
            }else{
                printf("\t\t\tSTATEMEN BERNILAI FALSE\n");
            }
            sprintf(current_line, "");
            index_row_now++;
        }
    }
    else if(!strcmp(cmd, "USE")) {
        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand: [%s]\n", command);
        put_commandlogs(command);

        char *nama_database = "";
        nama_database = uppercase(trimStr(getStrBetween(command, "DATABASE", ";")));
        printf("\tnama_database: %s\n", nama_database);

        // JANGAN LUPA CEK APAKAH DATABASENYA DAPAT DIGUNAKAN SAMA SI USER?
        // JANGAN LUPA BUAT CHECK PERMISSION
        char temp_auth_string[SIZE];
        sprintf(temp_auth_string, "%s%s%s%s%s", GLOBAL_USERNAME, GLOBAL_DELIMITER, GLOBAL_PASSWORD, GLOBAL_DELIMITER, nama_database);

        if(!AUTH(temp_auth_string) && !GLOBAL_IS_ROOT){
            // MAAF ANDA TIDAK PUNTA IJIN UNTUK DROP DB INI
            sprintf(GLOBAL_REPLY_MSG, "%sERROR:: You don't have permission to USE [%s]\r\n", GLOBAL_REPLY_MSG, nama_database);
            return;
        }

        // START THE THING
        sprintf(GLOBAL_DATABASE, "%s", nama_database);
        sprintf(GLOBAL_REPLY_MSG, "%sNOW USING DATABASE [%s]\n", GLOBAL_REPLY_MSG, nama_database);
    }
    else if(!strcmp(cmd, "GRANT")) {
        put_commandlogs(command);

        // JANGAN LUPA cek ROOT!
        if(!GLOBAL_IS_ROOT){
            sprintf(GLOBAL_REPLY_MSG, "ERROR:: You don't have permission to Grant permission\r\n");
            return;
        }

        printf("\nCommand Detected: [%s]\n", cmd);
        printf("\tcommand: [%s]\n", command);

        // Information
        char *nama_database = "";
        char *username = "";
        char *password = "";

        nama_database = uppercase(trimStr(getStrBetween(command, "PERMISSION", "INTO")));
        username = trimStr(getStrBetween(command, "INTO", ";"));

        printf("\tnama_database: [%s]\n", nama_database);
        printf("\tusername: [%s]\n", username);

        // JAN LUPA GET PASSWORD DARI USER YANG MAU DI INPUTIN
        char file_tabel[SIZE]; sprintf(file_tabel, "databases/%s/%s", GLOBAL_DATABASE, "USERS");
        FILE* fptr_db = fopen(file_tabel, "r");

        char current_line[SIZE];
        bool found = false;
        int start = 0;
        while (fgets(current_line, sizeof(current_line), fptr_db)) {
            printf("\t\t\tcurrent_line :: %s\n", current_line);
            if(start == 0){
                start++;
                continue;
            }

            char *temp_username = getStrBetween(current_line, NULL, GLOBAL_DELIMITER);
            printf("\t\t\ttemp_username :: %s\n", temp_username);
            if(!strcmp(temp_username, username)){
                printf("\t\t\t[%s] == [%s]", temp_username, username);
                char temp_user_pp[SIZE]; sprintf(temp_user_pp, "%s%s", temp_username, GLOBAL_DELIMITER);
                char *after_username = getStrBetween(current_line, temp_user_pp, NULL);
                password = getStrBetween(after_username, NULL, GLOBAL_DELIMITER);
                found = true;
                break;
            }
        }

        if(!found){
            sprintf(GLOBAL_REPLY_MSG, "%sERROR:: Username [%s] does not exist!", GLOBAL_REPLY_MSG, username);
            return;
        }

        // execute :: USE DATABASE INFORMATION_SCHEMA;
        sprintf(GLOBAL_QUERY, "USE DATABASE INFORMATION_SCHEMA;"); execute();
        sprintf(GLOBAL_QUERY, "INSERT INTO USERS ('%s', '%s', '%s');", username, password, nama_database); execute();        
    }
}

int main(int argc, char const *argv[]) {
	getcwd(GLOBAL_CWD, sizeof(GLOBAL_CWD));

    // Make dir databases
    mkdir("databases", "0777");

	int server_fd;
	struct sockaddr_in server;
	char *msg_in;
	
	// buat fd
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("Server Error: Socket not created succesfully");
		exit(EXIT_FAILURE);
	}
	
	// Initialize
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	// Bind
	bind(server_fd, (struct sockaddr *)&server, sizeof(server));

	// Set listent
	listen(server_fd, 3);

	int fd_in;
	int addrlen = sizeof(server);
	getpeername(server_fd, (struct sockaddr*)&server, (socklen_t*)&addrlen);  
	printf("Host created: ip %s:%d \n",inet_ntoa(server.sin_addr) , ntohs(server.sin_port));

	while(true){
        GLOBAL_IS_ROOT = false;
		fd_in = accept(server_fd, (struct sockaddr*) &server, (socklen_t*)&addrlen);
		getpeername(server_fd, (struct sockaddr*)&server, (socklen_t*)&addrlen);
        
		sprintf(GLOBAL_LOG_MSG, "[!] New client connected [%d] >> %s:%d", fd_in, inet_ntoa(server.sin_addr) , ntohs(server.sin_port));
		logs();

        // CHECK AUTH DULU
        char *auth_str = readClient(fd_in);
        char *temp_get_username = getStrBetween(auth_str, NULL, GLOBAL_DELIMITER);
        char temp_username_delimiter[SIZE];
        sprintf(temp_username_delimiter, "%s%s", temp_get_username, GLOBAL_DELIMITER);
        char *temp_get_password = getStrBetween(auth_str, temp_username_delimiter, GLOBAL_DELIMITER);

        sprintf(GLOBAL_USERNAME, "%s", temp_get_username);
        sprintf(GLOBAL_PASSWORD, "%s", temp_get_password);

        sprintf(GLOBAL_LOG_MSG, "[%d] RECEIVE USERNAME [%s] WITH PASSWORD [%s]", fd_in, GLOBAL_USERNAME, GLOBAL_PASSWORD);
        logs();

        if(!strcmp(GLOBAL_USERNAME, "ROOT")){
            GLOBAL_IS_ROOT = true;
            sprintf(GLOBAL_DATABASE, "INFORMATION_SCHEMA");
        }

        if(AUTH(auth_str) || GLOBAL_IS_ROOT){
            sprintf(GLOBAL_LOG_MSG, "[%d] >> AUTH : [%s] >> SUCCESS", fd_in, auth_str);
            logs();

            sprintf(GLOBAL_REPLY_MSG, "Y");
            reply(fd_in, GLOBAL_REPLY_MSG);
        }else{
            sprintf(GLOBAL_LOG_MSG, "[%d] >> AUTH : [%s] >> FAILED", fd_in, auth_str);
            logs();

            sprintf(GLOBAL_REPLY_MSG, "N");
            reply(fd_in, GLOBAL_REPLY_MSG);
            close(fd_in);
            continue;
        }

		reply(fd_in, "Selamat datang di server!\r\n[<] ");
        char message[SIZE]; strcpy(message, "FIRST");

        while(true){
            char *response = readClient(fd_in);
            sprintf(message, "%s", response);
            if(!strcmp(lowercase(message), "exit")){
                break;
            }

            sprintf(GLOBAL_QUERY, "%s", response);

            sprintf(GLOBAL_REPLY_MSG, "");
            if(!strcmp(trimStr(response), "")){
                sprintf(GLOBAL_REPLY_MSG, "%s[<] ", GLOBAL_REPLY_MSG);
                reply(fd_in, GLOBAL_REPLY_MSG);
                continue;
            }
            if(safe_run(execute)){
                sprintf(GLOBAL_LOG_MSG, "[%d] >> execute success", fd_in);
                logs();
            }else{
                sprintf(GLOBAL_LOG_MSG, "[%d] >> execute failed", fd_in);
                logs();
                sprintf(GLOBAL_REPLY_MSG, "EXECUTE FAILED\r\n");
            }
            
            sprintf(GLOBAL_REPLY_MSG, "%s[<] ", GLOBAL_REPLY_MSG);
            reply(fd_in, GLOBAL_REPLY_MSG);            
        }

		getpeername(server_fd, (struct sockaddr*)&server, (socklen_t*)&addrlen);
		sprintf(GLOBAL_LOG_MSG, "Client disconnected [%d] >> %s:%d", fd_in, inet_ntoa(server.sin_addr) , ntohs(server.sin_port));
		logs();

		close(fd_in);
	}
	return 0;
}