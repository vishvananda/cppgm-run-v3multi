struct C { ~decltype(x)(); };

struct Attr {
  inline __attribute__((always_inline)) virtual __attribute__((visibility("hidden"))) ~Attr();
};

template<class T> struct Box { ~Box<T>(); };

int ::operator+(int value) { return value; }

template<class T> struct Holder {};
extern template struct Holder<int>;
