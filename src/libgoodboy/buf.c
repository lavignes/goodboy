#include <goodboy/buf.h>

#include <stdlib.h>
#include <string.h>

View const VIEW_NULL = {NULL, 0};

Bool viewEquals(View lhs, View rhs) {
    if (lhs.len != rhs.len) {
        return FALSE;
    }
    return memcmp(lhs.bytes, rhs.bytes, lhs.len) == 0;
}

void bufFini(Buf* buf) {
    if (buf->view.bytes) {
        free(buf->view.bytes);
    }
    memset(buf, 0, sizeof(*buf));
}
