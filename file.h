#ifndef _FILE_H_
#  define _FILE_H_

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

typedef enum			/* File/URL access events */
{
  HD_FILE_LOCAL,		/* About to check for a local file */
  HD_FILE_CACHE,		/* Serving a URL from the web cache */
  HD_FILE_DATA,			/* About to decode a "data:" URI */
  HD_FILE_REQUEST,		/* About to send a HTTP request */
  HD_FILE_REDIRECT,		/* Following a HTTP redirection */
  HD_FILE_RESULT		/* Finished a HTTP request */
} hd_file_event_t;

typedef int (*hd_file_cb_t)(void *data, hd_file_event_t event,
			    const char *url, const char *localname,
			    int status);
				/* File/URL access callback, returns 0 to deny
				 * access and 1 to allow it */

extern const char	*file_basename(const char *s);
extern void		file_callback(hd_file_cb_t cb, void *data);
extern void		file_cleanup(void);
extern const char	*file_directory(const char *s);
extern const char	*file_find(const char *path, const char *s);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_FILE_H_ */
