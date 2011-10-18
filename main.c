//
//  main.c
//  cstream
//
//  Created by William Sobel on 10/17/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include "stream.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


static int HandleXmlChunk(const char *xml)
{
  xmlDocPtr document;
  int i;
  char *path;
  xmlXPathContextPtr xpathCtx;
  xmlNodePtr root;
  xmlXPathObjectPtr nodes;
  xmlNodeSetPtr nodeset;
  
  document = xmlReadDoc(BAD_CAST xml, "file://node.xml",
                        NULL, XML_PARSE_NOBLANKS);
  if (document == NULL) 
  {
    fprintf(stderr, "Cannot parse document: %s\n", xml);
    xmlFreeDoc(document);
    return 0;
  }
  
  path = "//m:Events/*|//m:Samples/*|//m:Condition/*";
  xpathCtx = xmlXPathNewContext(document);
  
  root = xmlDocGetRootElement(document);
  if (root->ns != NULL)
  {
    xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href);
  }
  else
  {
    fprintf(stderr, "Document does not have a namespace: %s\n", xml);
    xmlFreeDoc(document);
    return 0;
  }
  
  // Evaluate the xpath.
  nodes = xmlXPathEval(BAD_CAST path, xpathCtx);
  if (nodes == NULL || nodes->nodesetval == NULL)
  {
    printf("No nodes found matching XPath\n");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(document);
    return 1;
  }
  
  // Spin through all the events, samples and conditions.
  nodeset = nodes->nodesetval;
  for (i = 0; i != nodeset->nodeNr; ++i)
  {
    xmlNodePtr n = nodeset->nodeTab[i];
    xmlChar *name = xmlGetProp(n, BAD_CAST "name");
    xmlChar *value;
    
    if (name == NULL)
      name = xmlGetProp(n, BAD_CAST "dataItemId");
    value = xmlNodeGetContent(n);
    
    printf("Found: %s:%s with value %s\n", 
           n->name, name, value);
    xmlFree(value);
    xmlFree(name);
  }
  
  xmlXPathFreeObject(nodes);    
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(document);
  
  return 1;
}

static void StreamXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
{
  va_list args;
  char buffer[2048];
  va_start(args, msg);
  vsnprintf(buffer, 2046, msg, args);
  buffer[2047] = '0';
  va_end(args);
  
  fprintf(stderr, "XML Error: %s\n", buffer);   
}



int main(int argc, char *argv[])
{
  void *context;
  
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <url>\n", argv[0]);
    exit(1);
  }
  
  xmlInitParser();
  xmlXPathInit();
  xmlSetGenericErrorFunc(NULL, StreamXMLErrorFunc);
  
  context = MTCStreamInit(argv[1], HandleXmlChunk);
  MTCStreamStart(context);
  MTCStreamFree(context);
  
  return 0;
}
