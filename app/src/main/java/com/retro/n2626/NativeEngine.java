package com.retro.n2626;

public final class NativeEngine {
    static {
        System.loadLibrary("unicorn");
        System.loadLibrary("nokia_engine");
    }

    public static native String testCPU();
    public static native String runImportedTest(byte[] code);
    public static native String traceFirmware(byte[] image, boolean fast);
    public static native String traceFull(byte[] decoded, byte[] raw, boolean explore);
}
