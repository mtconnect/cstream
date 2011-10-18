#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <curl/curl.h>

#include "stream.h"

#define BLOCK_SIZE (2048 * 1024)
#define BOUNDARY_SIZE 48

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef WIN32
#define strncasecmp _strnicmp
#else
#define strncpy_s(d, i, s, c) strncpy(d, s, c); 
#endif

struct _XmlBlock {
  char block[BLOCK_SIZE];
  char boundary[BOUNDARY_SIZE];
  char *start;
  char *end;
  size_t length;
  size_t boundaryLength;
  char in_body;
  CURL *context;
  int stopped;
  MTCStreamXMLHandler handler;
};

typedef struct _XmlBlock XmlBlock;

#ifdef WIN32
/* Find a string in another string doing a case insensitive search */
static char *strcasestr(const char *s, const char *f)
{
  const char *sp = s;
  const char *fp = f;
  
  while (*sp != '\0')
  {
    if (tolower(*sp) == tolower(*fp))
      fp++;
    else
      fp = f;
    if (*fp == '\0')
      return (char*) (sp - strlen(f));
    sp++;
  }
  
  return NULL;
}
#endif


static size_t HandleHeader(char *ptr, size_t size, size_t nmemb, void *data)
{
  char line[512];
  XmlBlock *block = (XmlBlock*) data;
  
  int len = size * nmemb;
  if (len > 511) len = 511;
  memcpy(line, ptr, len);
  line[len] = '\0';
  
  /* All we care about is the content type. */
  if (strncasecmp(line, "content-type:", 12) == 0)
  {
    char *bp;
    
    if (strstr(line, "multipart/x-mixed-replace") == NULL)
    {
      fprintf(stderr, "Incorrect content type: '%s', must be multipart/x-mixed-replace\n",
              line);
      exit(1);
    }
    
    bp = strstr(ptr, "boundary=");
    if (bp == NULL)
    {
      fprintf(stderr, "Cannot find boundary in %s\n", line);
      exit(1);
    }
    
    block->boundary[0] = block->boundary[1] = '-';
    strncpy_s(block->boundary + 2, BOUNDARY_SIZE - 3, bp + 9, BOUNDARY_SIZE - 3);
    block->boundary[BOUNDARY_SIZE - 1] = '\0';
    bp = block->boundary + (strlen(block->boundary) - 1);
    while (!isalnum(*bp))
      *bp-- = '\0';
    
    block->boundaryLength = strlen(block->boundary);
    
    printf("Found boundary: %s\n", block->boundary);
  }
  return size * nmemb;
}

static size_t HandleData(char *ptr, size_t size, size_t nmemb, void *data)
{
  /* First find the boundary in the current block. */
  XmlBlock *block = (XmlBlock*) data;
  int need_data;
  
  if (block->stopped) return 0;
  
  if (block->boundary[0] == '\0')
  {
    fprintf(stderr, "Data arrived without boundary\n");
    exit(1);
  }

  /* Check for buffer overflow. returning a smaller number will cause this 
     to error out */
  if (size * nmemb > (size_t) (BLOCK_SIZE - (block->end - block->block)))
    return BLOCK_SIZE - (block->end - block->block);
  
  /* append the new data to the end of the block and null terminate */
  memcpy(block->end, ptr, size * nmemb);
  block->end += size * nmemb;
  *(block->end) = '\0';
  
  do
  {
    need_data = TRUE;
    if (!block->in_body)
    {
      // Look for the boundary
      char *bp = strstr(block->start, block->boundary);
      if (bp != NULL)
      {
        char *ep = strstr(bp, "\r\n\r\n");
        if (ep != NULL)
        {
          // Parse the headers after the boundary for the content length.
          char *cp;
          bp += block->boundaryLength + 2;
          cp = strcasestr(bp, "Content-length:");
          if (cp != NULL) {
            block->length = atoi(cp + 16);
          }
          
          // Scan for the "\r\n\r\n"
          block->in_body = 1;
          block->start = ep + 4;
        }
      }
    }
    
    if (block->in_body && (size_t) (block->end - block->start) >= (size_t) block->length)
    {
      char *ep;
      size_t len;
      int res;

      *(block->start + block->length) = '\0';
      
      /* We have a new chunk of xml data... */
      res = block->handler(block->start);
      if (res == 0) return 0;
      
      /* Consume the block and reset the pointers. */
      ep = block->start + block->length;
      len = block->end - ep;
      if (len > 0) memcpy(block->block, ep, len);
      block->start = block->block;
      block->end = block->block + len;
      block->length = 0;
      block->in_body = 0;
      
      if (len > 60) need_data = FALSE;
    }
  } while (!need_data);
  
  return size * nmemb;
}


// API Methods
void *MTCStreamInit(const char *aUrl, MTCStreamXMLHandler aHandler)
{
  XmlBlock *data = (XmlBlock*) calloc(1, sizeof(XmlBlock));
  data->start = data->end = data->block;
  
  data->context = curl_easy_init();
  data->handler = aHandler;

  //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(data->context, CURLOPT_URL, aUrl);
  curl_easy_setopt(data->context, CURLOPT_WRITEFUNCTION, HandleData);
  curl_easy_setopt(data->context, CURLOPT_WRITEDATA, data);
  curl_easy_setopt(data->context, CURLOPT_HEADERFUNCTION, HandleHeader);
  curl_easy_setopt(data->context, CURLOPT_WRITEHEADER, data);

  return data;
}

void MTCStreamStop(void *aContext)
{
  XmlBlock *data = (XmlBlock*) aContext;
  data->stopped = 1;
  
}

void MTCStreamStart(void *aContext)
{
  XmlBlock *data = (XmlBlock*) aContext;
  curl_easy_perform(data->context);
}

void MTCStreamFree(void *aContext)
{
  /* Assumes the perform is now complete */
  free(aContext);
}
