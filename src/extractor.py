import re
import sys

anyEntriesFound = False

# read lines until a non-empty one is found,
# return that line
def SkipUntilContent(f):
    skip = True
    while (skip == True):
      candidateLine = f.readline()
      if candidateLine != "\n":
        return candidateLine

# read lines until an empty is found
def SkipUntilEmpty(f):
    skip = True
    while (skip == True):
      skippedLine = f.readline()
      if skippedLine == "\n":
        skip = False

# process one copy entry from the map file
def processEntry(f, inputLine, file):
  # get the main title line
  if "lz77" not in inputLine:
    print("    Skipping non lz77 entry")
    # not lz77, skip
    SkipUntilEmpty(f)
  else:
    #lz77, read the line to get number of entries
    reg  = re.compile(r'\d+')
    reg2 = re.compile(r'\b0x[0-9A-F]+\b', re.IGNORECASE)
    numEntries = (int)(reg.findall(f.readline())[0])
    # read all entries
    ranges = []
    for i in range(1,numEntries+1):
      #read the line
      numbers = reg2.findall(f.readline())
      #process the numbers
      start = int(numbers[0],16)
      size  = int(numbers[1],16)
      ranges.append((start, size))

    # read the destination line to get expected size
    expected = int(reg2.findall(f.readline())[0],16)
    # then skip the destinations
    SkipUntilEmpty(f)

    # We now have everything we want, output the option

    # Get the range as one string
    rangeStr = ""
    count = 0;
    for (s1,s2) in ranges:
      if (count > 0):
         rangeStr += f","
      rangeStr += f"{s1:x}:{s2:x}"
      count += 1

    # Output the file name, the range, the expected
    # and an optional v(erbose)
    print(f"    {file} {rangeStr} {expected:x} v")
    anyEntriesFound = True

def get_syms(l):
  str = ""
  i = 0
  for x in l:
    if (i>0):
       str += ","
    str += f"{x}"
    i += 1
  return str

# Entry point

args = len(sys.argv);
if (args < 2):
  print("No file specified")
  sys.exit(1)

fileName = sys.argv[1];
try:
    with open (fileName, "r") as f:
        if (f.closed == True):
           print(f"Unable to open {fileName}")
           sys.exit(1)

        found = False
        ELFfile = ""
        # skip first line of ####
        f.readline()
        # Locate ELF output name
        while (found == False):
            line = f.readline()
            if "###" in line:
               found = True
               continue
            if "-o" in line:
                # found -o
               found = True
               line2 = f.readline();
               lineTot = line + line2;
               lineTot = lineTot.replace('\n', ' ')
               lineTot = lineTot.replace('#', ' ')
               reg  = re.compile(r'-o.*')
               res = reg.findall(lineTot)
               ELFfile = res[0].split()[1]

        found = False
        # Locate INIT TABLE
        while (found == False):
            line = f.readline()
            if not line:
              break;
            if "*** INIT TABLE" not in line:
              continue
            else:
              found = True

        if (found == False):
            print("INIT TABLE not found")
            sys.exit(1)

        # Skip until we get entries
        found = False
        while (found == False):
              line = f.readline()
              if not line:
                break;
              if "-------      ----" not in line:
                continue
              else:
                found = True

        if (found == False):
            print("-------      ---- not found")
            sys.exit(1)

        print("Found INIT TABLE map section, listing entries")
        if (ELFfile==""):
           ELFfile = "a.out"
           print("No ELF-file name found, using a.out")
        done = False;
        while (done == False):
            line = SkipUntilContent(f)
            if "***" not in line:
               processEntry(f, line, ELFfile)
            else:
               done = True
        print("No more entries found");

        # Locate the ENRY LIST
        found = False
        while (found == False):
            line = f.readline()
            if not line:
              break;
            if "*** ENTRY LIST" not in line:
              continue
            else:
              found = True
        if (found == True):
           print("Scanning ENTRY LIST")
           syms = []
           f.readline() #skip ***
           SkipUntilEmpty(f)
           f.readline()   # Entry ...
           f.readline()   # ----- ...
           done = False
           while (done == False):
             line = f.readline()
             if (line == "\n"):
                break;
             symbol = line.split()[0]
             if "lz77" in symbol:
               syms.append(symbol)
           if (len(syms) > 0):
              syms_str = get_syms(syms)
              print(f"Some lz77 symbols found: {syms_str}")
           else:
              if (anyEntriesFound == False):
                  print("No init entries and no symbols, this application does not use lz77")
              else:
                  print("No lz77 symbols found")
except IOError:
    print(f"Unable to open {fileName}")
    sys.exit(1)


