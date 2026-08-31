# TSA-000001985 - LZ77 init problem detection

## Summary

Support files for the TSA "Detecting LZ77 init problems": the `iz77check` C
tool, the `extractor.py` helper script, and two worked example projects used
to verify both tools against real linker output.

Full article: https://mypages.iar.com/s/article/Detecting-LZ77-init-problems

## Using the examples

Each example under `examples/` is a small linked project's output (a map
file and the matching ELF), plus captured command lines and their expected
output, showing the two-step detection workflow end to end:

1. **Run `extractor.py` on the map file** to find any lz77-compressed init
   batches and get a ready-to-use `iz77check` command line for each one.

   ```
   python3 src/extractor.py examples/lz77_2/lz77_2-copy.map
   ```

2. **Run `iz77check` on the ELF file** using the command line `extractor.py`
   produced, to confirm the batch decompresses cleanly. 
   Get the zipped binary for your OS from the project's GitHub Releases page 
   or build it from `src/iz77check.c`, e.g., 
   
   ```
   iz77check examples/lz77_2/lz77_2-copy.out 2200:5 100 v
   ```

For each example folder:

* `*.sh` / `*.cmd` - the exact commands to run (Linux / Windows).
* `*.sh.txt` / `*.cmd.txt` - the expected output of those commands, captured
  ahead of time, for comparison against what you get when you run them
  yourself.

`examples/lz77_2` is a clean case (no problem detected). `examples/lz77_3`
has two lz77-batches: the first demonstrates the actual problem this TSA
describes, the second is clean - walk through both the same way, using
`iz77check_first_line.*` and `iz77check_second_line.*`.


Prebuilt binaries for both platforms are published under the project's
[GitHub Releases](../../releases) (tagged versions, e.g. `v1.0.0`) as
`iz77check-linux.zip` and `iz77check-windows.zip`, together with a
`SHA256SUMS.txt` for verifying the downloaded zips - they are not committed
to the repo. The C source is in `src/iz77check.c` if you'd rather build it
yourself.
