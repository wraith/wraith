#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <algorithm>
using namespace std;

int main(int argc, char *argv[]) {
  if (argc == 2)
    return 1;

  fstream file;
  ofstream out;
  file.open(argv[1]);
  out.open(argv[2]);


  char line[1024] = "";
  string type;
  char c;

  while (file.getline(line, sizeof(line))) {
    if (line[0] == ':') {
      type = &line[1];
      if (type == "end")
        break;

      transform(type.begin(), type.end(), type.begin(), (int(*)(int)) toupper);

      type = "DEFAULT_" + type;
      out << "#define " << type << " \"\\" << endl;
    } else {
      if (!type.length()) 
        continue;

      out << line;
      c = file.peek();
      if (strchr("\n:", c)) {
        out << "\"\n" << endl;
        type = "";
      } else
        out << ",\\" << endl;
    }
  }

  file.close();
  out.close();
}
