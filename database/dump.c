#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>
#include <stdbool.h>
#include <dirent.h>

#define SIZE 1024

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

char* convertToCharPtr(char *str){
	int len=strlen(str);
	char* ret = malloc((len+1) * sizeof(char));
	for(int i=0; i<len; i++){
		ret[i] = str[i];
	}
	ret[len] = '\0';
	return ret;
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

bool AUTH(char *AUTH_STRING){
    char file_tabel[SIZE];
    sprintf(file_tabel, "../database/databases/%s/%s", "INFORMATION_SCHEMA", "USERS");
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

int main(int argc, char const *argv[]) {
    // printf("Running client as not root\n");
    if(argc == 6){
        if(!strcmp(argv[1], "-u") && !strcmp(argv[3], "-p")){
            sprintf(GLOBAL_USERNAME, "%s", argv[2]);
            sprintf(GLOBAL_PASSWORD, "%s", argv[4]);
            sprintf(GLOBAL_DATABASE, "%s", uppercase(argv[5]));
        }else{
            printf("Your argument is invalid");
            exit(0);
        }
    }else{
        printf("Your argument is invalid");
        exit(0);
    }

    // printf("GLOBAL_USERNAME : %s\r\n", GLOBAL_USERNAME);
    // printf("GLOBAL_PASSWORD : %s\r\n", GLOBAL_PASSWORD);
    // printf("GLOBAL_DATABASE : %s\r\n", GLOBAL_DATABASE);

    char temp_auth_string[SIZE];
    sprintf(temp_auth_string, "%s%s%s%s%s", GLOBAL_USERNAME, GLOBAL_DELIMITER, GLOBAL_PASSWORD, GLOBAL_DELIMITER, GLOBAL_DATABASE);

    // printf("temp_auth_string : %s\r\n", temp_auth_string);

    if(!AUTH(temp_auth_string)){
        // printf("%sERROR:: You don't have permission to USE [%s]\r\n", GLOBAL_DATABASE);
        exit(0);
    }else{
        // printf("AUTH :: SUCCESS\n");
    }

    char file_path[SIZE];
    sprintf(file_path, "../database/databases/%s", GLOBAL_DATABASE);
    // printf("file_path : %s\r\n", file_path);

    DIR *d;
    struct dirent *dir;
    d = opendir(file_path);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")){
                continue;
            }
            char TABLE_NAME[SIZE];
            sprintf(TABLE_NAME, dir->d_name);
            char table_path[SIZE];
            sprintf(table_path, "../database/databases/%s/%s", GLOBAL_DATABASE, TABLE_NAME);

            FILE* fptr_db = fopen(table_path, "r");
            char header_line[SIZE];
            fgets(header_line, sizeof(header_line), fptr_db);

            char header_name_set[100][SIZE]; int index_header = 0;
            char header_name_type[100][SIZE];

            char *tok_header = strtok(header_line, GLOBAL_DELIMITER);
            while(tok_header != NULL) {
                char *value_name = trimStr(getStrBetween(tok_header, NULL, "|"));
                char *value_type = trimStr(getStrBetween(tok_header, "|", NULL));
                
                sprintf(header_name_set[index_header], "%s", value_name);
                sprintf(header_name_type[index_header], "%s", value_type);
                
                tok_header = strtok(NULL, GLOBAL_DELIMITER);
                index_header++;
            }

            fclose(fptr_db);
            fptr_db = fopen(table_path, "r");
            
            char current_line[SIZE];
            int rownum = 0;
            while(fgets(current_line, sizeof(current_line), fptr_db)){
                current_line[strcspn(current_line, "\r\n")] = 0;
                if(rownum == 0){
                    printf("DROP TABLE %s;\n", TABLE_NAME);
                    printf("CREATE TABLE %s (", TABLE_NAME);

                    for(int i=0; i<index_header; i++){
                        if(i!=0){
                            printf(",%s %s", header_name_set[i], header_name_type[i]);
                        }else{
                            printf("%s %s", header_name_set[i], header_name_type[i]);
                        }
                    }
                    printf(")\n\n");
                }else{
                    printf("INSERT INTO %s (", TABLE_NAME);

                    char *tok = strtok(current_line, GLOBAL_DELIMITER);
                    int index_value = 0;
                    while(tok != NULL) {
                        if(index_value != 0){
                            if(!strcmp(header_name_type[index_value], "INT")){
                                printf(",%s", tok);
                            }else{
                                printf(",'%s'", tok);
                            }
                        }else{
                            if(!strcmp(header_name_type[index_value], "INT")){
                                printf("%s", tok);
                            }else{
                                printf("'%s'", tok);
                            }
                        }
                        tok = strtok(NULL, GLOBAL_DELIMITER);
                        index_value++;
                    }
                    printf(")\n");
                }
                rownum++;
            }
            fclose(fptr_db);
            printf("\n\n");
        }
        closedir(d);
    }
    return(0);

    return 0;
}