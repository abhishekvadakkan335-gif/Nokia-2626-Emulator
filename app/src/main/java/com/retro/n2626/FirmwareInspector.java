package com.retro.n2626;

import java.io.InputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.Locale;

/** Read-only inspection; no execution, extraction or persistent storage. */
final class FirmwareInspector {
    private static final long LIMIT=32L*1024*1024;
    private static final char[] HEX="0123456789abcdef".toCharArray();
    private static String hex(byte[] bytes,int count,boolean spaces) {
        StringBuilder out=new StringBuilder();
        for(int i=0;i<count;i++){
            if(spaces&&i>0)out.append(i%16==0?'\n':' ');
            int b=bytes[i]&255;
            out.append(HEX[b>>>4]).append(HEX[b&15]);
        }
        return out.toString();
    }
    static final class Inspection {
        final String report; final byte[] test, firmware;
        Inspection(String report, byte[] test,byte[] firmware){this.report=report;this.test=test;this.firmware=firmware;}
    }
    static Inspection inspect(InputStream in,String name) throws Exception {
        MessageDigest hash=MessageDigest.getInstance("SHA-256");
        byte[] header=new byte[64],buffer=new byte[8192];
        int headerSize=0,n; long total=0;
        java.io.ByteArrayOutputStream container=null;
        while((n=in.read(buffer))!=-1){
            if(n==0){int one=in.read();if(one==-1)break;buffer[0]=(byte)one;n=1;}
            if(total+n>LIMIT)throw new IOException("File exceeds 32 MiB inspection limit");
            int copy=Math.min(n,header.length-headerSize);
            if(copy>0){System.arraycopy(buffer,0,header,headerSize,copy);headerSize+=copy;}
            if(total==0&&(buffer[0]&255)==0xa2)container=new java.io.ByteArrayOutputStream();
            if(container!=null)container.write(buffer,0,n);
            hash.update(buffer,0,n);
            total+=n;
        }
        if(total==0)throw new IOException("Selected file is empty");
        String kind="Unknown binary; firmware format not verified";
        if(headerSize>=4&&header[0]==0x7f&&header[1]=='E'&&header[2]=='L'&&header[3]=='F')kind="ELF executable container";
        else if(headerSize>=4&&header[0]=='P'&&header[1]=='K'&&((header[2]==3&&header[3]==4)||(header[2]==5&&header[3]==6)||(header[2]==7&&header[3]==8)))kind="ZIP-family archive";
        else if(headerSize>=2&&(header[0]&255)==0x1f&&(header[1]&255)==0x8b)kind="GZIP compressed stream";
        String lower=name.toLowerCase(Locale.ROOT);
        boolean modelHint=lower.contains("rm291")||lower.contains("rm-291");
        String hint=lower.endsWith(".mcusw")||lower.endsWith(".mcu")?"MCU software candidate":lower.endsWith(".ppm")||lower.contains(".ppm_")?"PPM resource candidate":lower.endsWith(".cnt")||lower.contains(".image_")||lower.endsWith(".image")?"CNT/content candidate":lower.endsWith(".bin")?"BIN filename":"No recognized suffix";
        if(modelHint)hint+="; RM-291 filename match (not content verification)";
        byte[] known={1,0,(byte)0xa0,(byte)0xe3,2,0,(byte)0x80,(byte)0xe2};
        byte[] test=total==known.length && java.util.Arrays.equals(java.util.Arrays.copyOf(header,headerSize),known)?java.util.Arrays.copyOf(header,headerSize):null;
        String digest=hex(hash.digest(),32,false);
        String report="File: "+name+"\nSize: "+total+" bytes\nFormat: "+kind+"\nHint: "+hint+
            "\n\nSHA-256:\n"+digest+"\n\nFirst "+headerSize+" bytes:\n"+hex(header,headerSize,true)+
            "\n\nRead-only inspection. Nothing executed. CPU type, load address and entry point are not determined."+(test!=null?"\nKnown ARM diagnostic sample recognized.":"");
        byte[] firmware=null;
        if(container!=null){try{
            Dct4Container.Decoded decoded=Dct4Container.analyze(container.toByteArray());
            report+="\n\n"+decoded.report;
            if(digest.equals("05e988ec1c87df13ede1cab96b2f498cc50ae26fdbc3c03c98ff3161c2706ee6"))firmware=decoded.image;
            if(firmware!=null)report+="\nExperimental startup trace available for this validated sample.";
        }catch(java.io.IOException e){report+="\n\nDCT4 parser rejected file: "+e.getMessage();}}
        return new Inspection(report,test,firmware);
    }
}
