struct MoveAssigned
{
  int value;
  MoveAssigned& operator=(MoveAssigned&&);
};

void take(MoveAssigned value)
{
}

int main()
{
  MoveAssigned source;
  MoveAssigned copy = source;
  take(source);
  return source.value;
}
