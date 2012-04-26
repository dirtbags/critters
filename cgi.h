#ifndef __CGI_H__
#define __CGI_H__

#define POST_MAX 65536

int cgi_init(char *global_argv[]);
size_t cgi_item(char *str, size_t maxlen);
void cgi_header(char *content_type);

#endif
