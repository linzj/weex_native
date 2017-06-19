#include "base/files/scoped_file.h"
#include "base/logging.h"

namespace base {
namespace internal {

void ScopedFDCloseTraits::Free(int fd) {
  // It's important to crash here.
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  int ret = close(fd);

  // TODO(davidben): Remove this once it's been determined whether
  // https://crbug.com/603354 is caused by EBADF or a network filesystem
  // returning some other error.
  int close_errno = errno;

  CHECK(0 == ret);
}
}
}
