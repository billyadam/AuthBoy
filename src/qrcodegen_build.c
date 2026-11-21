// Builds the vendored, unmodified QR Code generator (third_party/qrcodegen) for the GBA.
// NDEBUG: its assert() checks would need the C library's assert handler, which Butano doesn't link.
#define NDEBUG
#include "../third_party/qrcodegen/qrcodegen.c"
