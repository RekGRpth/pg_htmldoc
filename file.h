#ifndef _FILE_H_
#  define _FILE_H_

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */

const char	*file_basename(const char *s);
void		file_cleanup(void);
const char	*file_directory(const char *s);
const char	*file_find(const char *path, const char *s);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_FILE_H_ */
