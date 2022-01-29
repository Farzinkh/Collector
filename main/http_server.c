/* HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <mbedtls/base64.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include <inttypes.h>

#include "http_server.h"

static const char *TAG = "HTTP_SERVER";

extern QueueHandle_t xQueueHttp;

#define STORAGE_NAMESPACE "www"

esp_err_t save_key_value(char * key, char * value)
{
	nvs_handle_t my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Write
	err = nvs_set_str(my_handle, key, value);
	if (err != ESP_OK) return err;

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	err = nvs_commit(my_handle);
	if (err != ESP_OK) return err;

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t load_key_values(char * key, char * value, size_t size)
{
	nvs_handle_t my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Read
	size_t _size = size;
	err = nvs_get_str(my_handle, key, value, &_size);
	ESP_LOGD(TAG, "nvs_get_str err=%d", err);
	if (err != ESP_OK) return err;
	ESP_LOGD(TAG, "err=%d key=[%s] value=[%s] _size=%d", err, key, value, _size);

	// Close
	nvs_close(my_handle);
	//return ESP_OK;
	return err;
}

int find_value(char * key, char * parameter, char * value) 
{
	//char * addr1;
	char * addr1 = strstr(parameter, key);
	if (addr1 == NULL) return 0;
	ESP_LOGD(TAG, "addr1=%s", addr1);

	char * addr2 = addr1 + strlen(key);
	ESP_LOGD(TAG, "addr2=[%s]", addr2);

	char * addr3 = strstr(addr2, "&");
	ESP_LOGD(TAG, "addr3=%p", addr3);
	if (addr3 == NULL) {
		strcpy(value, addr2);
	} else {
		int length = addr3-addr2;
		ESP_LOGD(TAG, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
		strncpy(value, addr2, length);
		value[length] = 0;
	}
	ESP_LOGI(TAG, "key=[%s] value=[%s]", key, value);
	return strlen(value);
}

static esp_err_t Text2Html(httpd_req_t *req, char * filename) {
	ESP_LOGI(TAG, "Reading %s", filename);
	FILE* fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	} else {
		char line[128];
		while (fgets(line, sizeof(line), fhtml) != NULL) {
			size_t linelen = strlen(line);
			//remove EOL (CR or LF)
			for (int i=linelen;i>0;i--) {
				if (line[i-1] == 0x0a) {
					line[i-1] = 0;
				} else if (line[i-1] == 0x0d) {
					line[i-1] = 0;
				} else {
					break;
				}
			}
			ESP_LOGD(TAG, "line=[%s]", line);
			esp_err_t ret = httpd_resp_sendstr_chunk(req, line);
			if (ret != ESP_OK) {
				ESP_LOGE(TAG, "httpd_resp_sendstr_chunk fail %d", ret);
			}
		}
		fclose(fhtml);
	}
	return ESP_OK;
}

// Calculate the size after conversion to base64
// http://akabanessa.blog73.fc2.com/blog-entry-83.html
int32_t calcBase64EncodedSize(int origDataSize)
{
	// Number of blocks in 6-bit units (rounded up in 6-bit units)
	int32_t numBlocks6 = ((origDataSize * 8) + 5) / 6;
	// Number of blocks in units of 4 characters (rounded up in units of 4 characters)
	int32_t numBlocks4 = (numBlocks6 + 3) / 4;
	// Number of characters without line breaks
	int32_t numNetChars = numBlocks4 * 4;
	// Size considering line breaks every 76 characters (line breaks are "\ r \ n")
	//return numNetChars + ((numNetChars / 76) * 2);
	return numNetChars;
}

esp_err_t Image2Base64(char * imageFileName, char * base64FileName)
{
	struct stat st;
	if (stat(imageFileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", imageFileName);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "%s st.st_size=%ld", imageFileName, st.st_size);

	// Allocate image memory
	unsigned char*	image_buffer = NULL;
	size_t image_buffer_len = st.st_size;
	image_buffer = malloc(image_buffer_len);
	if (image_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. image_buffer_len %d", image_buffer_len);
		return ESP_FAIL;
	}

	// Read image file
	FILE * fp_image = fopen(imageFileName,"rb");
	if (fp_image == NULL) {
		ESP_LOGE(TAG, "[%s] fopen fail.", imageFileName);
		free(image_buffer);
		return ESP_FAIL;
	}
	for (int i=0;i<st.st_size;i++) {
		fread(&image_buffer[i], sizeof(char), 1, fp_image);
	}
	fclose(fp_image);

	// Allocate base64 memory
	int32_t base64Size = calcBase64EncodedSize(st.st_size);
	ESP_LOGI(TAG, "base64Size=%d", base64Size);

	unsigned char* base64_buffer = NULL;
	size_t base64_buffer_len = base64Size + 1;
	base64_buffer = malloc(base64_buffer_len);
	if (base64_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. base64_buffer_len %d", base64_buffer_len);
		return ESP_FAIL;
	}

	// Convert from JPEG to BASE64
	size_t encord_len;
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, image_buffer, st.st_size);
	ESP_LOGI(TAG, "mbedtls_base64_encode=%d encord_len=%d", ret, encord_len);

	// Write Base64 file
	FILE * fp_base64 = fopen(base64FileName,"w");
	if (fp_base64 == NULL) {
		ESP_LOGE(TAG, "[%s] open fail", base64FileName);
		return ESP_FAIL;
	}
	fwrite(base64_buffer,base64_buffer_len,1,fp_base64);
	fclose(fp_base64);

	free(image_buffer);
	free(base64_buffer);
	return ESP_OK;
}

int SD_getFreeSpace(uint32_t *tot, uint32_t *free) {
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    /* Get volume information and free clusters of drive 0 */
    if(f_getfree("0:", &fre_clust, &fs) == FR_OK)
    {
        /* Get total sectors and free sectors */
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;

        *tot = tot_sect / 2;
        *free = fre_sect / 2;
        //ESP_LOGI(TAG, "%"PRIu32" KiB total drive space. %"PRIu32" KiB available.", *tot, *free);
        return *free/1024;
    }
    return ESP_FAIL;
}

esp_err_t Image2Html(httpd_req_t *req, char * filename, char * type)
{
	FILE * fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	}else{
		char  buffer[64];

		if (strcmp(type, "jpeg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "jpg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "png") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		} else {
			ESP_LOGW(TAG, "file type fail. [%s]", type);
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		}
		while(1) {
			size_t bufferSize = fread(buffer, 1, sizeof(buffer), fhtml);
			ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
			if (bufferSize > 0) {
				httpd_resp_send_chunk(req, buffer, bufferSize);
			} else {
				break;
			}
		}
		fclose(fhtml);
		httpd_resp_sendstr_chunk(req, "\">");
	}
	return ESP_OK;
}

/* HTTP get handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);
	char key[64];
	char parameter[128];
	esp_err_t err;

	// Send HTML header
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, CONFIG_MEDIA_DIR"HEAD.htm");

	httpd_resp_sendstr_chunk(req, "<body>");
	httpd_resp_sendstr_chunk(req, "<h1>Client server</h1>");

	strcpy(key, "framesize");
	char radio[16] = {0};
	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		find_value("radio=", parameter, radio);
	}

	httpd_resp_sendstr_chunk(req, "<h2>FRAMESIZE</h2>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/post\">");
	if (strcmp(radio, "5") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"5\" checked=\"checked\">QVGA");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"5\">QVGA");
	}
	if (strcmp(radio, "6") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"6\" checked=\"checked\">CIF");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"6\">CIF");
	}
	if (strcmp(radio, "7") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"7\" checked=\"checked\">HVGA");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"7\">HVGA");
	}
	if (strcmp(radio, "8") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"8\" checked=\"checked\">VGA");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"8\">VGA");
	}	
	if (strcmp(radio, "9") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"9\" checked=\"checked\">SVGA");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"9\">SVGA");
	}
	if (strcmp(radio, "11") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"11\" checked=\"checked\">HD");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"11\">HD");
	}
	if (strcmp(radio, "13") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"13\" checked=\"checked\">UXGA");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio\" value=\"13\">UXGA");
	}
	httpd_resp_sendstr_chunk(req, "<br>");
	httpd_resp_sendstr_chunk(req, "<input type=\"submit\" name=\"submit\" value=\"framesize\">");

	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		httpd_resp_sendstr_chunk(req, key);
		httpd_resp_sendstr_chunk(req, ":");
		httpd_resp_sendstr_chunk(req, parameter);
	}
	httpd_resp_sendstr_chunk(req, "</form><br>");
	httpd_resp_sendstr_chunk(req, "<h3>QVGA 320x240</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>CIF 400x296</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>HVGA 480x320</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>VGA 640x480</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>SVGA 800x600</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>HD 1280x720</h3>");
	httpd_resp_sendstr_chunk(req, "<h3>UXGA 1600x1200</h3>");
	
	strcpy(key, "pixformat");
	char radio2[16] = {0};
	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		find_value("radio2=", parameter, radio2);
	}

	httpd_resp_sendstr_chunk(req, "<h2>PIXEL FORMAT</h2>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/post\">");
	if (strcmp(radio2, "3") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"3\" checked=\"checked\">JPEG");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"3\">JPEG");
	}
	if (strcmp(radio2, "2") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"2\" checked=\"checked\">GRAYSCALE");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"2\">GRAYSCALE");
	}
	if (strcmp(radio2, "1") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"1\" checked=\"checked\">RGB565");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"1\">RGB565");
	}
	if (strcmp(radio2, "5") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"5\" checked=\"checked\">RAW");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio2\" value=\"5\">RAW");
	}
	httpd_resp_sendstr_chunk(req, "<br>");
	httpd_resp_sendstr_chunk(req, "<input type=\"submit\" name=\"submit\" value=\"pixformat\">");

	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		httpd_resp_sendstr_chunk(req, key);
		httpd_resp_sendstr_chunk(req, ":");
		httpd_resp_sendstr_chunk(req, parameter);
	}
	
	strcpy(key, "picspeed");
	char radio3[16] = {0};
	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		find_value("radio3=", parameter, radio3);
	}

	httpd_resp_sendstr_chunk(req, "<h2>PICTURE SPEED</h2>");
	httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/post\">");
	if (strcmp(radio3, "5000") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"5000\" checked=\"checked\">5sec");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"5000\">5sec");
	}
	if (strcmp(radio3, "3000") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"3000\" checked=\"checked\">3sec");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"3000\">3sec");
	}
	if (strcmp(radio3, "2000") == 0) {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"2000\" checked=\"checked\">2sec");
	} else {
		httpd_resp_sendstr_chunk(req, "<input type=\"radio\" name=\"radio3\" value=\"2000\">2sec");
	}
	httpd_resp_sendstr_chunk(req, "<br>");
	httpd_resp_sendstr_chunk(req, "<input type=\"submit\" name=\"submit\" value=\"picspeed\">");

	err = load_key_values(key, parameter, sizeof(parameter));
	ESP_LOGD(TAG, "%s=%d", key, err);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "parameter=[%s]", parameter);
		httpd_resp_sendstr_chunk(req, key);
		httpd_resp_sendstr_chunk(req, ":");
		httpd_resp_sendstr_chunk(req, parameter);
	}
	
	httpd_resp_sendstr_chunk(req, "</form><br>");
	uint32_t total,free,available;
    available=SD_getFreeSpace(&total,&free);   
    if(available==ESP_FAIL){
        ESP_LOGE(TAG,"Faild to read free space");
    } else {
        char full[255]="";
        char availables[11];
        char starttag[]="<h3>Sdcard ";
        char endtag[]=" MB available.</h3>";
        strcat(full,starttag);
        snprintf(availables,sizeof availables, "%" PRIu32,available);
        strcat(full,availables);
        strcat(full,endtag);
        httpd_resp_sendstr_chunk(req, full);
    }
    

    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/post\">");
	httpd_resp_sendstr_chunk(req, "<button name=\"apply\" value=\"1\" type=\"submit\">Apply new changes</button>");
	httpd_resp_sendstr_chunk(req, "<button name=\"camera\" value=\"switch\" type=\"submit\">Camera switch</button>");
	httpd_resp_sendstr_chunk(req, "<button name=\"download\" value=\"1\" type=\"submit\">Download Database</button><br>");
	httpd_resp_sendstr_chunk(req, "</form><br>");

	/* Send Image to HTML file */
	Image2Html(req, CONFIG_MEDIA_DIR"ESP-LOGO.txt", "png");

	/* Send remaining chunk of HTML file to complete it */
	httpd_resp_sendstr_chunk(req, "</body></html>");

	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/* HTTP post handler */
static esp_err_t root_post_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_post_handler req->uri=[%s]", req->uri);
	ESP_LOGI(TAG, "root_post_handler content length %d", req->content_len);
	char*  buf = malloc(req->content_len + 1);
	size_t off = 0;
	while (off < req->content_len) {
		/* Read data received in the request */
		int ret = httpd_req_recv(req, buf + off, req->content_len - off);
		if (ret <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				httpd_resp_send_408(req);
			}
			free (buf);
			return ESP_FAIL;
		}
		off += ret;
		ESP_LOGI(TAG, "root_post_handler recv length %d", ret);
	}
	buf[off] = '\0';
	ESP_LOGI(TAG, "root_post_handler buf=[%s]", buf);

	URL_t urlBuf;
	find_value("submit=", buf, urlBuf.url);
	ESP_LOGI(TAG, "urlBuf.url=[%s]", urlBuf.url);
	//strcpy(urlBuf.url, "submit4");
	//strcpy(urlBuf.parameter, &req->uri[9]);
	strcpy(urlBuf.parameter, buf);
	if (xQueueSend(xQueueHttp, &urlBuf, portMAX_DELAY) != pdPASS) {
		ESP_LOGE(TAG, "xQueueSend Fail");
	}
	free(buf);

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "post successfully");
	return ESP_OK;
}



/* Function to start the web server */
esp_err_t start_server(const char *base_path, int port)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for get */
	httpd_uri_t _root_get_handler = {
		.uri	   = "/",
		.method    = HTTP_GET,
		.handler   = root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_get_handler);

	/* URI handler for post */
	httpd_uri_t _root_post_handler = {
		.uri	   = "/post",
		.method    = HTTP_POST,
		.handler   = root_post_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_post_handler);


	return ESP_OK;
}


void http_server_task(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s:%d", task_parameter, CONFIG_WEB_PORT);

	// Start Server
	ESP_LOGI(TAG, "Starting server on %s", url);
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	
	URL_t urlBuf;
	while(1) {
		// Waiting for submit
		if (xQueueReceive(xQueueHttp, &urlBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "url=%s", urlBuf.url);
			ESP_LOGI(TAG, "parameter=%s", urlBuf.parameter);
			if (strcmp(urlBuf.parameter,"apply=1")==0){
				ESP_LOGI(TAG, "apply clicked");
			} else if (strcmp(urlBuf.parameter,"camera=switch")==0){
				ESP_LOGI(TAG, "camera clicked");
			} else if (strcmp(urlBuf.parameter,"download=1")==0) {
				ESP_LOGI(TAG, "download clicked");
			}
			// save key & value to NVS
			esp_err_t err = save_key_value(urlBuf.url, urlBuf.parameter);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error (%s) saving to NVS", esp_err_to_name(err));
			}
		}
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
}
