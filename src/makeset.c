#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <cctype>
#include <algorithm>
#include <cstring>
using namespace std;

int main(int argc, char *argv[]) {
  if (argc == 2)
    return 1;

  bd::Stream file, out;
  file.loadFile(argv[1]);

  bd::String type, line;
  char c;

  while (file.tell() < file.length()) {
    line = file.getline().chomp();
    if (line[0] == '#') continue;
    if (line[0] == ':') {
      type = line(1);
      if (type == "end")
        break;

      transform(type.begin(), type.end(), type.mdata(), (int(*)(int)) toupper);

      type = "DEFAULT_" + type;
      out << "#define " << type << " \"\\" << "\n";
    } else {
      if (!type.length()) 
        continue;

      out << line;
      c = file.peek()[0];
      if (strchr("\n:", c)) {
        out << "\"\n\n";
        type = "";
      } else
        out << ",\\\n";
    }
  }

  out.writeFile(argv[2]);
}
