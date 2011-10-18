//
//  stream.h
//  cstream
//
//  Created by William Sobel on 10/17/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#ifndef cstream_stream_h
#define cstream_stream_h


#ifdef  __cplusplus
extern "C" {
#endif
  
#if 0
}
#endif
  
/* Return 1 if successful, 0 to stop. */
typedef int (*MTCStreamXMLHandler)(const char *aXML);
  
void *MTCStreamInit(const char *aUrl, MTCStreamXMLHandler aHandler);

void MTCStreamStart(void *aContext);

void MTCStreamStop(void *aContext);

void MTCStreamFree(void *aContext);
  
#ifdef  __cplusplus
}
#endif

#endif
