package com.retro.n2626;

import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Host-only runner; no firmware bundled or downloaded by CI. */
public final class NativeEngine {
    static { System.loadLibrary("nokia_engine"); }
    public static native String testCPU();
    public static native String traceFirmware(byte[] image, boolean fast);
    public static void main(String[] args) throws Exception {
        if (args.length == 1 && args[0].equals("--self-test")) {
            String report = testCPU();
            System.out.println(report);
            if (report.contains("FAIL") || !report.contains("PASS")) System.exit(1);
            return;
        }
        if (args.length < 1 || args.length > 2) {
            System.err.println("Usage: NativeEngine <locally decoded RM-291 flash> [--fast]");
            System.exit(2);
        }
        byte[] image = Files.readAllBytes(Path.of(args[0]));
        String digest = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(image));
        if (!digest.equals("1909ad142c0609e53596c2affdf5c23fc8e00a37f87a16718b488e6ea5beaddf"))
            throw new IllegalArgumentException("Unexpected decoded image digest: " + digest);
        boolean fast = args.length == 2 && args[1].equals("--fast");
        System.out.println(traceFirmware(image, fast));
    }
}
