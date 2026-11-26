# Local host diagnostic runner

The host workflow compiles the same JNI CPU engine for Linux x86-64 using Unicorn 2.1.4.
It builds and tests only open-source code and synthetic CPU diagnostics. The proprietary
firmware is not included in the repository or CI artifacts.

After extracting the host artifact, run on a compatible Linux host with Java 17+:

```sh
java -Djava.library.path=. -cp classes com.retro.n2626.NativeEngine /path/to/decoded-flash.bin --fast
```

Omit `--fast` to use the earlier scoped diagnostic model. Fast mode is a bounded,
speculative probe, not a validated hardware model. Compare against strict results;
never interpret callback count as boot percentage.
