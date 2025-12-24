#ifndef _FILE_H_
#  define _FILE_H_

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

extern const char	*file_basename(const char *s);
extern void		file_cleanup(void);
extern const char	*file_directory(const char *s);
extern const char	*file_find(const char *path, const char *s);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_FILE_H_ */
