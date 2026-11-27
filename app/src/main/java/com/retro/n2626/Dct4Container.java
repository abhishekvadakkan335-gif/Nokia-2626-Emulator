package com.retro.n2626;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.nio.charset.StandardCharsets;

/**
 * Strict RM-291 A2 / 0x14 record parser, validated against a local 06.80 sample.
 * Public flash transform adapted from sp00nznet/nokia-recomp recompiler/dct4decrypt.py
 * (MIT, see docs/third-party/nokia-recomp-LICENSE.txt), based on g3gg0 DCT4Crypt.
 * No firmware bytes bundled; no guest execution or persistent storage.
 */
final class Dct4Container {
    private static final int BASE=0x01000000, LIMIT=0x02000000, CLEAR=0x84;
    private static final int[] BIT={0x1221,0xa91a,0x52a5,0x0908,0xa918,0x1020,0xffff,0x52a1,0x0100,0x1220,0xad1a,0x0900,0x1000,0x2908,0x5221,0xa908};
    private static final int[] ADDR={0x0fae,0x3e7f,0xc99f,0xd6f7,0xa71b,0x14c4,0x52a5,0xcbb1,0x4285,0xefdf,0xdff7,0x5080,0xee9f,0,0x8432,0x5221,0x4084,0xa91a,0x56e7,0xb93a,0x5b21,0xa818,0,0xefdf};
    private static final int[][] ADJ={{0x00140,0x1000},{0x00220,0x52a1},{0x00480,0x1221},{0x00600,0xb928},{0x00810,0x5221},{0x00840,0x1220},{0x00900,0x2008},{0x01020,0x1221},{0x01080,0x0908},{0x01100,0x52a1},{0x02020,0x0100},{0x02080,0xfbbd},{0x04010,0xa91a},{0x04040,0xa908},{0x08008,0x2908},{0x09000,0x1000},{0x0a000,0xbd3a},{0x10010,0xad1a},{0x10040,0x5221},{0x10400,0x0908},{0x20200,0x53a5},{0x40040,0xa91a},{0x44000,0x1b20},{0x80100,0xa918},{0x800000,0xb908}};
    private static int u(byte[] b,int p){return b[p]&255;}
    private static long be32(byte[] b,int p){return ((long)u(b,p)<<24)|((long)u(b,p+1)<<16)|(u(b,p+2)<<8)|u(b,p+3);}
    private static String hex(long n){return String.format(Locale.ROOT,"0x%08X",n);}
    private static int find(byte[] data,String s,int start,int end){
        byte[] pat=s.getBytes(StandardCharsets.US_ASCII);
        outer: for(int i=start;i<=end-pat.length;i++){for(int j=0;j<pat.length;j++)if(data[i+j]!=pat[j])continue outer;return i;}return -1;
    }
    private static int[] inverse(){
        int[] table=new int[65536];
        for(int x=0;x<65536;x++){int y=0;for(int j=0;j<16;j++)if((x&(1<<j))!=0)y^=BIT[j];table[y]=x;}
        return table;
    }
    private static int transform(int word,int address,int[] inverse){
        for(int[] a:ADJ)if((address&a[0])==a[0])word^=a[1];
        for(int j=0;j<24;j++)if((address&(1<<(j+1)))!=0)word^=ADDR[j];
        return inverse[word];
    }
    // Matching 06.81 archive only; verify each component and combined image.
    static final class Full {
        final byte[] decoded,raw;
        Full(byte[] decoded,byte[] raw){this.decoded=decoded;this.raw=raw;}
    }
    private static String sha256(byte[] b) throws Exception {
        byte[] h=java.security.MessageDigest.getInstance("SHA-256").digest(b);
        StringBuilder s=new StringBuilder();for(byte v:h)s.append(String.format(Locale.ROOT,"%02x",v&255));return s.toString();
    }
    static Full fullZip(java.io.InputStream in) throws Exception {
        String[] suffix={"mcusw","ppm_ej","image_ej"};
        String[] digests={"3b8bb5b503f7deba62ccc187e74eeea3d65c2b7d358c8d0137604449c8fbce36","2240a70dc6bbb6b224e49a3c19ef0d223a31d08988a041582aafc6a643073db1","05825a81f5639ba72a70a2fc9e1b5457356682a8b300d171ffd00fdf9986e1d3"};
        int[] starts={BASE,0x018e0000,0x01b80000}, ends={0x01897154,0x01a78118,0x01d60000};
        byte[][] components=new byte[3][];
        long total=0;int entries=0;
        try(java.util.zip.ZipInputStream z=new java.util.zip.ZipInputStream(in)){
            java.util.zip.ZipEntry ent;
            while((ent=z.getNextEntry())!=null){
                if(++entries>128)throw new IOException("Too many archive entries");
                int k=-1;
                for(int i=0;i<3;i++)if(ent.getName().equals("Nokia_2626_RM-291/Firmware/rm291__06.81."+suffix[i]))k=i;
                java.io.ByteArrayOutputStream out=k<0?null:new java.io.ByteArrayOutputStream();
                byte[] buf=new byte[16384];int r;long entryBytes=0;
                while((r=z.read(buf))!=-1){
                    total+=r;entryBytes+=r;
                    if(total>32L*1024*1024||entryBytes>16L*1024*1024)throw new IOException("Archive exceeds size limit");
                    if(out!=null)out.write(buf,0,r);
                }
                if(k>=0){if(components[k]!=null)throw new IOException("Duplicate component");components[k]=out.toByteArray();}
                z.closeEntry();
            }
        }
        byte[] raw=new byte[0xd60000];Arrays.fill(raw,(byte)255);
        ArrayList<int[]> mcu=new ArrayList<>();int count=0;
        for(int k=0;k<3;k++){
            byte[] b=components[k];
            if(b==null||!sha256(b).equals(digests[k])||b.length<5||u(b,0)!=0xa2)throw new IOException("Missing or unexpected 06.81 "+suffix[k]);
            long head=5+be32(b,1);
            if(head<5||head>b.length||find(b,"DCT4",5,(int)head)<0)throw new IOException("Bad header");
            int p=(int)head,last=starts[k];boolean first=true;
            while(p<b.length){
                if(++count>774||b.length-p<10||u(b,p)!=0x14)throw new IOException("Unexpected record");
                long addr=be32(b,p+1);int len=(u(b,p+6)<<16)|(u(b,p+7)<<8)|u(b,p+8);
                if((first&&addr!=starts[k])||addr<last||addr+len>ends[k]||len<=0||len>b.length-p-10||((addr|len)&1)!=0)throw new IOException("Bad record range");
                int h=0;for(int i=p+1;i<p+10;i++)h+=u(b,i);
                int pay=u(b,p+5);for(int i=p+10;i<p+10+len;i++)pay+=u(b,i);
                if((h&255)!=255||(pay&255)!=0)throw new IOException("Record checksum mismatch");
                System.arraycopy(b,p+10,raw,(int)addr-BASE,len);
                if(k==0)mcu.add(new int[]{(int)addr-BASE,len});
                last=(int)addr+len;p+=10+len;first=false;
            }
            if(last!=ends[k])throw new IOException("Component end mismatch");
        }
        if(count!=774||!sha256(raw).equals("7687b11edab6e1588b376bf5510aae31e4252c6156ecf4f9049efe209802e11b"))throw new IOException("Raw flash hash mismatch");
        byte[] decoded=raw.clone();int[] inv=inverse();
        int param=transform((u(raw,CLEAR)<<8)|u(raw,CLEAR+1),BASE+CLEAR,inv)^0xffff;
        for(int[] r:mcu)for(int i=Math.max(CLEAR,r[0]);i<r[0]+r[1];i+=2){
            int w=transform((u(raw,i)<<8)|u(raw,i+1),BASE+i,inv)^param;
            decoded[i]=(byte)(w>>>8);decoded[i+1]=(byte)w;
        }
        if(!sha256(decoded).equals("410eda8d7d17570b31758aadb98dba956adcb70028ae2e011abf88d19e65181b"))throw new IOException("Decoded flash hash mismatch");
        return new Full(decoded,raw);
    }
    static final class Decoded {
        final String report; final byte[] image;
        Decoded(String report,byte[] image){this.report=report;this.image=image;}
    }
    static Decoded analyze(byte[] b) throws IOException {
        if(b.length<5||u(b,0)!=0xa2)throw new IOException("Not an A2 container");
        long header=5+be32(b,1);
        if(header<5||header>b.length||find(b,"DCT4",5,(int)header)<0)throw new IOException("Invalid or non-DCT4 header");
        int p=(int)header, end=BASE, total=0;
        ArrayList<int[]> records=new ArrayList<>();
        while(p<b.length){
            if(records.size()>=8192||b.length-p<10||u(b,p)!=0x14)throw new IOException("Unsupported record at "+hex(p));
            long address=be32(b,p+1);
            int n=(u(b,p+6)<<16)|(u(b,p+7)<<8)|u(b,p+8);
            if(address<end||address+n>LIMIT||n<=0||n>b.length-p-10||((address|n)&1)!=0)throw new IOException("Invalid block range at "+hex(p));
            int h=0;for(int i=p+1;i<p+10;i++)h+=u(b,i);
            if((h&255)!=255)throw new IOException("Header checksum failed at "+hex(p));
            int payload=u(b,p+5);for(int i=p+10;i<p+10+n;i++)payload+=u(b,i);
            if((payload&255)!=0)throw new IOException("Payload checksum failed at "+hex(p));
            records.add(new int[]{(int)address,n,p+10});
            end=(int)address+n;total+=n;p+=10+n;
        }
        if(records.isEmpty()||records.get(0)[0]!=BASE||end-BASE<CLEAR+2)throw new IOException("Not supported MCU base/range");
        byte[] image=new byte[end-BASE];Arrays.fill(image,(byte)0xff);
        boolean autoWordPresent=false;
        for(int[] r:records){System.arraycopy(b,r[2],image,r[0]-BASE,r[1]);if(r[0]<=BASE+CLEAR&&r[0]+r[1]>=BASE+CLEAR+2)autoWordPresent=true;}
        if(!autoWordPresent)throw new IOException("Missing descramble reference word");
        int[] inv=inverse();
        int parameter=transform((u(image,CLEAR)<<8)|u(image,CLEAR+1),BASE+CLEAR,inv)^0xffff;
        for(int[] r:records)for(int i=Math.max(CLEAR,r[0]-BASE);i<r[0]-BASE+r[1];i+=2){
            int word=transform((u(image,i)<<8)|u(image,i+1),BASE+i,inv)^parameter;
            image[i]=(byte)(word>>>8);image[i+1]=(byte)word;
        }
        int model=find(image,"RM-291",0,image.length);
        int nokia=find(image,"Nokia",0,image.length);
        int midp=find(image,"Profile/MIDP-2.0",0,image.length);
        String identity="Not verified";
        if(model>=0&&nokia>=0){
            int version=find(image,"V ",Math.max(0,model-48),model);
            if(version>=0){int stop=model+6;while(stop<image.length&&stop<model+48&&u(image,stop)>=32&&u(image,stop)<=126)stop++;
                identity=new String(image,version,stop-version,StandardCharsets.US_ASCII).trim();}
            else identity="RM-291 marker found";
        }
        boolean startup=image.length>=0x7f084 && be32(image,0x100)==0xea01fbbeL && be32(image,0x7f000)==0xe10f4000L && model>=0;
        String report="DCT4 MCU container: VALID\nRecords: "+records.size()+"\nHeader/payload checksums: all passed\nFlash: "+hex(BASE)+" to "+hex(end)+" (exclusive)\nPayload: "+total+" bytes\nUnpopulated gap: "+(image.length-total)+" bytes (filled FF)\nDecoded identity:\n"+identity+"\nMIDP-2.0 marker: "+(midp>=0?"found":"not found")+"\nNo firmware executed; peripherals still unknown.";
        if(startup)report+="\nCandidate startup: 0x01000100 -> 0x0107F000 (big-endian ARM).";
        return new Decoded(report,startup?image:null);
    }
}
