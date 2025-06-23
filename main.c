#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <cjson/cJSON.h>
#include <getopt.h>

// Buffer size constants
#define MAX_FILENAME 256
#define MAX_TAG 256
#define MAX_DATE 32
#define MAX_FORMAT 8
#define MAX_PATH 512
#define MAX_URL 512
#define MAX_FILEPATH 1024

// Structure to hold response data
struct http_response {
    char* data;
    size_t size;
};

// Structure to hold photo information
struct photo {
    char name[MAX_FILENAME];
    char tag[MAX_TAG];
    char date[MAX_DATE];  // Date extracted from "d" field
};

// Structure for progress tracking
struct progress_data {
    char filename[MAX_FILENAME];
};

// Structure for CLI options
struct cli_options {
    char format[MAX_FORMAT];      // "dng", "jpg", "all"
    char filename[MAX_FILENAME];  // Specific filename to download
    char target_path[MAX_PATH]; // Alternative target path
    int help;
};

// Function prototypes
void sanitize_filename(char* filename);
int validate_path(const char* path);

// Function to display help
void show_help(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Download photos from Ricoh GR II camera\n\n");
    printf("Options:\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -f, --format FORMAT   File format to download (dng, jpg, all) [default: all]\n");
    printf("  -F, --file FILENAME   Download only specified file\n");
    printf("  -p, --path PATH       Alternative target path [default: $HOME/Pictures/RicohGRII]\n");
    printf("\nExamples:\n");
    printf("  %s                    Download all photos\n", program_name);
    printf("  %s -f jpg            Download only JPG files\n", program_name);
    printf("  %s -f dng            Download only DNG files\n", program_name);
    printf("  %s -F R0001234.JPG   Download specific file\n", program_name);
    printf("  %s -p /media/usb     Download to USB drive\n", program_name);
}

// Function to parse command line arguments
int parse_arguments(int argc, char* argv[], struct cli_options* options) {
    int c;
    int option_index = 0;
    
    // Initialize default values
    strcpy(options->format, "all");
    options->filename[0] = '\0';
    options->target_path[0] = '\0';  // Empty means use default
    options->help = 0;
    
    static struct option long_options[] = {
        {"help",    no_argument,       0, 'h'},
        {"format",  required_argument, 0, 'f'},
        {"file",    required_argument, 0, 'F'},
        {"path",    required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    
    while ((c = getopt_long(argc, argv, "hf:F:p:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                options->help = 1;
                return 0;
            case 'f':
                if (strcmp(optarg, "dng") == 0 || strcmp(optarg, "jpg") == 0 || strcmp(optarg, "all") == 0) {
                    strcpy(options->format, optarg);
                } else {
                    fprintf(stderr, "Error: Invalid format '%s'. Use 'dng', 'jpg', or 'all'\n", optarg);
                    return -1;
                }
                break;
            case 'F':
                strncpy(options->filename, optarg, sizeof(options->filename) - 1);
                options->filename[sizeof(options->filename) - 1] = '\0';
                sanitize_filename(options->filename);
                if (strlen(options->filename) == 0) {
                    fprintf(stderr, "Error: Invalid filename after sanitization\n");
                    return -1;
                }
                break;
            case 'p':
                strncpy(options->target_path, optarg, sizeof(options->target_path) - 1);
                options->target_path[sizeof(options->target_path) - 1] = '\0';
                if (validate_path(options->target_path) != 0) {
                    fprintf(stderr, "Error: Invalid path '%s'\n", optarg);
                    return -1;
                }
                break;
            case '?':
                return -1;
            default:
                return -1;
        }
    }
    
    return 0;
}

// Function to check if file matches format filter
int matches_format(const char* filename, const char* format) {
    if (strcmp(format, "all") == 0) {
        return 1;
    }
    
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return 0;
    }
    ext++; // Skip the dot
    
    if (strcmp(format, "jpg") == 0) {
        return (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0);
    } else if (strcmp(format, "dng") == 0) {
        return (strcasecmp(ext, "dng") == 0);
    }
    
    return 0;
}

// Callback function to write received data into our buffer
static size_t write_callback(void* contents, size_t size, size_t nmemb, struct http_response* response) {
    size_t realsize = size * nmemb;
    char* ptr = realloc(response->data, response->size + realsize + 1);
    
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0; // Null terminate
    
    return realsize;
}

// Progress callback function
static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                           curl_off_t ultotal __attribute__((unused)), 
                           curl_off_t ulnow __attribute__((unused))) {
    struct progress_data* progress = (struct progress_data*)clientp;
    
    if (dltotal > 0) {
        double percentage = (double)dlnow / (double)dltotal * 100.0;
        printf("\r%s: %.1f%% (%.2f KB / %.2f KB)", 
               progress->filename, percentage, 
               (double)dlnow / 1024.0, (double)dltotal / 1024.0);
        fflush(stdout);
    }
    
    return 0; // Return 0 to continue download
}

// Function to create directory if it doesn't exist
int create_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

// Function to check if file exists
int file_exists(const char* filepath) {
    return access(filepath, F_OK) == 0;
}

// Function to sanitize filename by removing dangerous characters
void sanitize_filename(char* filename) {
    if (!filename) return;
    
    char* src = filename;
    char* dst = filename;
    
    while (*src) {
        // Allow alphanumeric, dots, hyphens, underscores
        if ((*src >= 'a' && *src <= 'z') ||
            (*src >= 'A' && *src <= 'Z') ||
            (*src >= '0' && *src <= '9') ||
            *src == '.' || *src == '-' || *src == '_') {
            *dst++ = *src;
        }
        // Skip dangerous characters
        src++;
    }
    *dst = '\0';
}

// Function to validate and sanitize path
int validate_path(const char* path) {
    if (!path || strlen(path) == 0) return -1;
    
    // Check for path traversal attempts
    if (strstr(path, "..") != NULL) return -1;
    if (strstr(path, "//") != NULL) return -1;
    
    // Check for null bytes
    if (strlen(path) != strcspn(path, "\0")) return -1;
    
    // Check maximum path length
    if (strlen(path) >= MAX_PATH) return -1;
    
    return 0;
}

// Function to convert timestamp to date folder format (YYYY-MM-DD)
void timestamp_to_date_folder(const char* timestamp, char* date_folder) {
    // Parse timestamp and convert to YYYY-MM-DD format
    struct tm tm_info = {0};
    time_t t;
    
    // Try to parse the timestamp (assuming format like "20241222T120000" or similar) 2025-06-07T09:32:40
    if (sscanf(timestamp, "%4d-%2d-%2dT%2d:%2d:%2d", 
               &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday,
               &tm_info.tm_hour, &tm_info.tm_min, &tm_info.tm_sec) >= 3) {
        tm_info.tm_year -= 1900;  // tm_year is years since 1900
        tm_info.tm_mon -= 1;      // tm_mon is 0-11
        
        snprintf(date_folder, MAX_DATE, "%04d-%02d-%02d", 
                tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);
    } else {
        // Fallback to current date if parsing fails
        t = time(NULL);
        struct tm* current_tm = localtime(&t);
        strftime(date_folder, MAX_DATE, "%Y-%m-%d", current_tm);
    }
}

// Function to download a single photo
int download_photo(const char* base_url, const char* name, const char* tag, const char* date_folder, const char* base_path) {
    CURL* curl;
    CURLcode res;
    FILE* fp;
    char url[MAX_URL];
    char filepath[MAX_FILEPATH];
    char full_dir_path[MAX_PATH];
    struct progress_data progress_data;
    
    // Validate input parameters
    if (!base_url || !name || !tag || !date_folder || !base_path) {
        fprintf(stderr, "Invalid parameters to download_photo\n");
        return -1;
    }
    
    // Validate base_path
    if (validate_path(base_path) != 0) {
        fprintf(stderr, "Invalid base path: %s\n", base_path);
        return -1;
    }
    
    // Create full directory path
    snprintf(full_dir_path, sizeof(full_dir_path), "%s/%s", base_path, date_folder);
    
    // Validate the full directory path
    if (validate_path(full_dir_path) != 0) {
        fprintf(stderr, "Invalid directory path: %s\n", full_dir_path);
        return -1;
    }
    
    // Create directory if it doesn't exist
    if (create_directory(full_dir_path) != 0) {
        return -1;
    }
    
    // Create full file path
    snprintf(filepath, sizeof(filepath), "%s/%s", full_dir_path, name);
    
    // Check if file already exists
    if (file_exists(filepath)) {
        printf("File already exists, skipping: %s\n", filepath);
        return 0;
    }
    
    // Create download URL
    snprintf(url, sizeof(url), "%s/v1/photos/%s/%s", base_url, tag, name);
    
    // Setup progress data
    strncpy(progress_data.filename, name, sizeof(progress_data.filename) - 1);
    progress_data.filename[sizeof(progress_data.filename) - 1] = '\0';
    
    // Open file for writing
    fp = fopen(filepath, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        // Enable progress callback
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        
        res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            printf("\n"); // New line after progress
            fprintf(stderr, "Download failed for %s: %s\n", name, curl_easy_strerror(res));
            fclose(fp);
            unlink(filepath); // Remove incomplete file
            curl_easy_cleanup(curl);
            return -1;
        }
        
        curl_easy_cleanup(curl);
    }
    
    fclose(fp);
    printf("\nCompleted: %s\n", filepath);
    return 0;
}

// Function to parse JSON response using cJSON
int parse_photos_json(const char* json_data, struct photo** photos, int* photo_count) {
    cJSON* json = cJSON_Parse(json_data);
    if (!json) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return -1;
    }
    
    cJSON* dirs = cJSON_GetObjectItemCaseSensitive(json, "dirs");
    if (!cJSON_IsArray(dirs)) {
        fprintf(stderr, "No 'dirs' array found in JSON\n");
        cJSON_Delete(json);
        return -1;
    }
    
    int capacity = 100;
    *photos = malloc(capacity * sizeof(struct photo));
    if (!*photos) {
        cJSON_Delete(json);
        return -1;
    }
    
    int count = 0;
    cJSON* dir;
    cJSON_ArrayForEach(dir, dirs) {
        cJSON* tag = cJSON_GetObjectItemCaseSensitive(dir, "name");
        if (!cJSON_IsString(tag) || !tag->valuestring) {
            continue; // Skip if name is not a string
        }

        cJSON* files = cJSON_GetObjectItemCaseSensitive(dir, "files");
        if (!cJSON_IsArray(files)) {
            continue;
        }
        
        cJSON* file;
        cJSON_ArrayForEach(file, files) {
            if (count >= capacity) {
                capacity *= 2;
                *photos = realloc(*photos, capacity * sizeof(struct photo));
                if (!*photos) {
                    cJSON_Delete(json);
                    return -1;
                }
            }
            
            // Extract name
            cJSON* name = cJSON_GetObjectItemCaseSensitive(file, "n");
            if (cJSON_IsString(name) && name->valuestring) {
                strncpy((*photos)[count].name, name->valuestring, sizeof((*photos)[count].name) - 1);
                (*photos)[count].name[sizeof((*photos)[count].name) - 1] = '\0';
                sanitize_filename((*photos)[count].name);
                
                strncpy((*photos)[count].tag, tag->valuestring, sizeof((*photos)[count].tag) - 1);
                (*photos)[count].tag[sizeof((*photos)[count].tag) - 1] = '\0';
                sanitize_filename((*photos)[count].tag);
                
                // Skip if filename becomes empty after sanitization
                if (strlen((*photos)[count].name) == 0) {
                    continue;
                }
            }
            
            // Extract date from "d" field
            cJSON* d = cJSON_GetObjectItemCaseSensitive(file, "d");
            if (cJSON_IsString(d) && d->valuestring) {
                timestamp_to_date_folder(d->valuestring, (*photos)[count].date);
            } else {
                // Fallback to current date
                time_t t = time(NULL);
                struct tm* tm_info = localtime(&t);
                strftime((*photos)[count].date, sizeof((*photos)[count].date), "%Y-%m-%d", tm_info);
            }
            
            count++;
        }
    }
    
    *photo_count = count;
    cJSON_Delete(json);
    return 0;
}

int main(int argc, char* argv[]) {
    CURL* curl;
    CURLcode res;
    struct http_response response;
    struct photo* photos = NULL;
    int photo_count = 0;
    char base_path[MAX_PATH];
    const char* home = getenv("HOME");
    struct cli_options options;
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &options) != 0) {
        return 1;
    }
    
    // Show help if requested
    if (options.help) {
        show_help(argv[0]);
        return 0;
    }
    
    // Determine target path
    if (options.target_path[0] != '\0') {
        // Use user-specified path (already validated in parse_arguments)
        snprintf(base_path, sizeof(base_path), "%s", options.target_path);
    } else {
        // Use default path
        if (!home) {
            fprintf(stderr, "Cannot get HOME environment variable\n");
            return 1;
        }
        snprintf(base_path, sizeof(base_path), "%s/Pictures/RicohGRII", home);
        
        // Validate the constructed default path
        if (validate_path(base_path) != 0) {
            fprintf(stderr, "Error: Invalid default path\n");
            return 1;
        }
    }
    
    printf("Target directory: %s\n", base_path);
    
    // Create base directory
    if (create_directory(base_path) != 0) {
        return 1;
    }
    
    // Initialize response structure
    response.data = malloc(1);
    response.size = 0;
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.1/_gr/objs");
        
        // Set callback function to handle response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Set timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        // Perform the request
        res = curl_easy_perform(curl);
        
        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse JSON and extract photo information
            if (parse_photos_json(response.data, &photos, &photo_count) == 0) {
                int downloaded = 0;
                printf("Found %d photos matching criteria\n", photo_count);
                
                // Download each photo based on filters
                for (int i = 0; i < photo_count; i++) {
                    // Check if specific filename is requested
                    if (options.filename[0] != '\0') {
                        if (strcmp(photos[i].name, options.filename) != 0) {
                            continue;
                        }
                    }
                    
                    // Check format filter
                    if (!matches_format(photos[i].name, options.format)) {
                        continue;
                    }
                    
                    printf("Photo %d: %s, date=%s\n", 
                           downloaded + 1, photos[i].name, photos[i].date);
                    
                    if (download_photo("http://192.168.0.1", photos[i].name, photos[i].tag, 
                                     photos[i].date, base_path) == 0) {
                        downloaded++;
                    }
                }
                
                printf("\nDownload complete. Downloaded %d photos to %s\n", downloaded, base_path);
                free(photos);
            } else {
                fprintf(stderr, "Failed to parse JSON response\n");
            }
        }
        
        // Cleanup curl
        curl_easy_cleanup(curl);
    }
    
    // Cleanup
    curl_global_cleanup();
    if (response.data) {
        free(response.data);
    }
    
    return 0;
}