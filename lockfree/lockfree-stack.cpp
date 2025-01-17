
#include "lockfree-stack.h"
int main()
{
  LockfreeStack<int> st;
  st.push(1);
  st.push(2);

  {
    auto value = st.pop();
    cout << *value << endl;
  }

  cout << boolalpha << st.empty() << endl;
}