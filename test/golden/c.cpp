#include "no_such_file.h"

void my_printf(char*);

class T {
  int t_method();
} t_instance;

class U {
  int u_method();
  void v_method();
  float w_method() { return 6.7; }
};

int U::u_method() {
  return 123;
}

void U::v_method() {}

class V;

struct S;

static void f(int a0, char a1);

int gx, *gy;

int hz;

typedef void (*a_function_ptr_t)(void* dst, void* src);

void (*my_funcy_ptr)() = NULL;

int my_main() {
  int x;
  while (1) {
    break;
  }
  my_printf("asdf\n");
  return 0;
}

namespace N {
int n0;
namespace {
int n1[];
namespace Ooo::Ppppp {
int** n2;
}
int n8;
}  // namespace
int n9;
}  // namespace N

void v(void) {}

int min_xy = min(x, y);

using std::make_pair;
std::pair<X, Y> pair_xy = make_pair(x, y);

namespace aargh {
#define BLAH_SIZE 99
int blah[BLAH_SIZE];
#undef BLAH_SIZE

typedef struct {
  int i, j;
  double d;
} thomson, thompson;

struct {
  int i, j;
  double d;
} dupond;

struct dupont_struct {
  int i, j;
  double d;
} dupont;

int inside;
}  // namespace aargh
int outside;

class Structor {
 public:
  Structor();
  Structor(int arg0) : field(arg0) {}
  Structor(int arg0, int arg1);
  ~Structor();

  Structor operator+(const Structor& other) const;
  int operator[](int index) { return field + index; }

 private:
  int field;
};

Structor::Structor() : field(123) {}
Structor::~Structor() {}

Structor Structor::operator+(const Structor& other) const {
  return Structor(45);
}

// https://fmt.dev/12.0/api/#format_to_n
template <typename OutputIt, typename... T>
auto format_to_n(OutputIt out, size_t n, format_string<T...> fmt, T&&... args)
    -> format_to_n_result<OutputIt>;

std::unique_ptr<struct S> Sssify(int x, int y) {
  int z;
  return nullptr;
}

struct S s_instance;

enum english_number { one, two, three } en;

typedef enum { un, deux, trois } fr;

enum class german_number { eins, zwei, drei };
