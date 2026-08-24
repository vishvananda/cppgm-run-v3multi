void f(int* p, int* q) {
  ::delete p;
  ::delete [] q;
}
