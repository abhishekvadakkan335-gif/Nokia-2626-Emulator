package com.retro.n2626;
import android.app.*;import android.os.*;import android.content.*;import android.net.Uri;import android.graphics.*;import android.view.*;import android.widget.*;import java.text.SimpleDateFormat;import java.util.*;
public class MainActivity extends Activity {
 Phone phone; public void onCreate(Bundle b){super.onCreate(b);getWindow().getDecorView().setSystemUiVisibility(5894);phone=new Phone();setContentView(phone);}
 void javaGames(){Intent i=getPackageManager().getLaunchIntentForPackage("ru.playsoftware.j2meloader");if(i!=null)startActivity(i);else new AlertDialog.Builder(this).setTitle("Java games").setMessage("Install J2ME Loader, then import your compatible JAR game there. Games run in the external emulator.").setPositiveButton("Get emulator",(d,w)->startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse("https://github.com/nikita36078/J2ME-Loader/releases")))).setNegativeButton("Back",null).show();}
 void showCpuTest(){
    final AlertDialog dialog=new AlertDialog.Builder(this).setTitle("ARM and memory diagnostics").setMessage("Running native CPU and memory self-test...").setPositiveButton("Close",null).create();
    dialog.show();
    new Thread(()->{String output;try{output=NativeEngine.testCPU();}catch(LinkageError|RuntimeException e){output="CPU engine unavailable: "+e.getClass().getSimpleName()+"\n"+e.getMessage();}final String result=output;runOnUiThread(()->{if(dialog.isShowing())dialog.setMessage(result+"\n\nExpected: all four checks PASS.\nSynthetic memory map only; not Nokia firmware boot.");});},"arm-self-test").start();
 }
 private static final int PICK_FIRMWARE=2626;
 void pickFirmware(){
    Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);
    i.addCategory(Intent.CATEGORY_OPENABLE);
    i.setType("*/*");
    try{startActivityForResult(i,PICK_FIRMWARE);}
    catch(ActivityNotFoundException e){new AlertDialog.Builder(this).setMessage("No Android file picker available.").setPositiveButton("Close",null).show();}
 }
 @Override protected void onActivityResult(int request,int result,Intent data){
    super.onActivityResult(request,result,data);
    if(request!=PICK_FIRMWARE||result!=RESULT_OK||data==null||data.getData()==null)return;
    final Uri uri=data.getData();
    final AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Firmware inspector").setMessage("Reading selected file...").setPositiveButton("Close",null).setNeutralButton("Run test",null).create();
    dialog.show();
    dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setVisibility(View.GONE);
    new Thread(()->{
        FirmwareInspector.Inspection inspected=null;
        String name="Selected file";
        String output;
        try{
            try(android.database.Cursor cursor=getContentResolver().query(uri,new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)){
                if(cursor!=null&&cursor.moveToFirst()&&!cursor.isNull(0))name=cursor.getString(0);
            }catch(Exception ignored){}
            try(java.io.InputStream stream=getContentResolver().openInputStream(uri)){
                if(stream==null)throw new java.io.IOException("File cannot be opened");
                inspected=FirmwareInspector.inspect(stream,name);
                output=inspected.report;
            }
        }catch(Exception e){output="Import failed: "+e.getMessage();}
        final String report=output;
        final byte[] test=inspected==null?null:inspected.test;
        final byte[] firmware=inspected==null?null:inspected.firmware;
        runOnUiThread(()->{
            if(!dialog.isShowing())return;
            dialog.setMessage(report);
            if(test==null&&firmware==null)return;
            Button run=dialog.getButton(AlertDialog.BUTTON_NEUTRAL);
            run.setText(firmware!=null?"Trace startup":"Run test");
            run.setVisibility(View.VISIBLE);
            run.setOnClickListener(v->{
                run.setEnabled(false);
                dialog.setMessage("Running bounded CPU trace...");
                new Thread(()->{
                    String value;
                    try{value=firmware!=null?NativeEngine.traceFirmware(firmware):NativeEngine.runImportedTest(test);}
                    catch(LinkageError|RuntimeException e){value="Execution error: "+e.getMessage();}
                    final String answer=value;
                    runOnUiThread(()->{if(dialog.isShowing()){dialog.setMessage(firmware!=null?answer:report+"\n\n"+answer);run.setEnabled(true);}});
                },"guest-trace").start();
            });
        });
    },"firmware-inspect").start();
 }
 class Phone extends View {
 Canvas c; Paint p=new Paint(); String page="Home",draft="",calc="",result=""; int selected=0,profile=0; String[] menus={"Messages","Contacts","Call log","Settings","Games","Organizer","Calculator","Radio","Profiles"}; String[] keys={"Menu","▲","Back","◀","OK","▶","Call","▼","End","1","2 ABC","3 DEF","4 GHI","5 JKL","6 MNO","7 PQRS","8 TUV","9 WXYZ","*","0","#"}; float scale,ox,oy; long last=0; String lastKey="";int tap=0; Handler h=new Handler();
 Phone(){super(MainActivity.this);setLayerType(View.LAYER_TYPE_SOFTWARE,null);h.postDelayed(new Runnable(){public void run(){invalidate();h.postDelayed(this,1000);}},1000);}
 void rect(float x,float y,float w,float z,int color,float r){p.setColor(color);c.drawRoundRect(x,y,x+w,y+z,r,r,p);} void text(String s,float x,float y,float size,int color){p.setColor(color);p.setTextSize(size);p.setTypeface(Typeface.create("sans-serif",Typeface.BOLD));c.drawText(s,x,y,p);} int col(String s){return Color.parseColor(s);}
 protected void onDraw(Canvas canvas){c=canvas;c.drawColor(col("#151a20"));scale=Math.min(getWidth()/360f,getHeight()/680f);ox=(getWidth()-360*scale)/2;oy=(getHeight()-680*scale)/2;c.save();c.translate(ox,oy);c.scale(scale,scale);rect(22,8,316,664,col("#aeb8bf"),48);rect(35,20,290,640,col("#344b65"),38);text("N O K I A",133,53,17,Color.WHITE);rect(53,68,254,254,Color.BLACK,8);c.save();c.translate(61,76);c.scale(238/128f,238/128f);c.clipRect(0,0,128,128);rect(0,0,128,128,col("#e7f1f8"),0);rect(0,0,128,15,col("#245a99"),0);text("▂▄▆  4G",3,11,8,Color.WHITE);text("▰",111,11,10,Color.WHITE);
 if(page.equals("Home")){rect(0,15,128,99,col("#a9d5eb"),0);text(new SimpleDateFormat("HH:mm",Locale.US).format(new Date()),29,57,25,col("#173a66"));text(new SimpleDateFormat("EEE, dd MMM",Locale.US).format(new Date()),25,73,10,col("#173a66"));text("Retro mobile",29,96,10,col("#173a66"));}
 else if(page.equals("Menu")){for(int i=0;i<9;i++){int x=3+i%3*42,y=18+i/3*31;if(i==selected)rect(x,y,40,30,col("#8ac2f4"),3);rect(x+13,y+2,15,15,col(new String[]{"#dca23a","#54a96c","#477ec2"}[i%3]),4);text(""+(i+1),x+18,y+13,9,Color.WHITE);text(menus[i],x+1,y+26,6,col("#152e48"));}}
 else {text(page,4,28,12,col("#173a66"));String body="";switch(page){case "Messages":body=draft+"_";break;case "Calculator":body=calc+"\n"+result+"\n* +   # =\nCall -   Menu clear";break;case "Games":body="OK: Java games\nvia J2ME Loader";break;case "Settings":body="Profile: "+(profile==0?"General":"Silent")+"\nOK: toggle profile\nMenu: CPU test\n#: inspect firmware";break;case "Profiles":body="Profile: "+(profile==0?"General":"Silent")+"\nOK to toggle";break;case "Organizer":body=new SimpleDateFormat("EEEE\ndd MMM yyyy",Locale.US).format(new Date());break;case "Contacts":body="No contacts\nLocal demo";break;case "Radio":body="FM hardware\nnot emulated";break;default:body="No call records";}int y=43;for(String line:body.split("\n",-1)){while(line.length()>23){text(line.substring(0,23),4,y,9,Color.BLACK);line=line.substring(23);y+=12;}text(line,4,y,9,Color.BLACK);y+=12;}}
 rect(0,114,128,14,col("#245a99"),0);text(page.equals("Home")?"Menu":"Select",3,124,9,Color.WHITE);text("Back",101,124,9,Color.WHITE);c.restore();for(int i=0;i<keys.length;i++){int x=49+i%3*89,y=335+i/3*43;rect(x,y,84,38,col("#192c40"),10);rect(x+2,y+2,80,32,col("#60758a"),8);text(keys[i],x+8,y+23,15,Color.WHITE);}c.restore();}
 public boolean onTouchEvent(android.view.MotionEvent e){if(e.getAction()!=MotionEvent.ACTION_DOWN)return true;float x=(e.getX()-ox)/scale,y=(e.getY()-oy)/scale;int a=(int)((x-49)/89),b=(int)((y-335)/43);if(x>=49&&x<316&&y>=335&&b<7&&a<3){press(keys[b*3+a]);performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);invalidate();}return true;}
 void press(String k){if(k.equals("End")){page="Home";return;}if(k.equals("Back")){if(page.equals("Messages")&&!draft.isEmpty())draft=draft.substring(0,draft.length()-1);else page=page.equals("Menu")?"Home":"Menu";return;}if(page.equals("Home")){if(k.equals("Menu")||k.equals("OK"))page="Menu";return;}if(page.equals("Menu")){if(k.equals("▲"))selected=(selected+6)%9;else if(k.equals("▼"))selected=(selected+3)%9;else if(k.equals("◀"))selected=(selected+8)%9;else if(k.equals("▶"))selected=(selected+1)%9;else if(k.equals("OK")||k.equals("Menu"))page=menus[selected];else if(Character.isDigit(k.charAt(0))&&k.charAt(0)!='0')page=menus[k.charAt(0)-'1'];return;}if(page.equals("Games")&&k.equals("OK")){javaGames();return;}if(page.equals("Settings")&&k.equals("#")){pickFirmware();return;}if(page.equals("Settings")&&k.equals("Menu")){showCpuTest();return;}if((page.equals("Settings")||page.equals("Profiles"))&&k.equals("OK")){profile=1-profile;return;}if(page.equals("Messages")&&Character.isDigit(k.charAt(0))){String[] letters={" 0",".,!?1","ABC2","DEF3","GHI4","JKL5","MNO6","PQRS7","TUV8","WXYZ9"};String s=letters[k.charAt(0)-'0'];long now=System.currentTimeMillis();if(k.equals(lastKey)&&now-last<900&&!draft.isEmpty()){tap=(tap+1)%s.length();draft=draft.substring(0,draft.length()-1);}else tap=0;draft+=s.charAt(tap);last=now;lastKey=k;return;}if(page.equals("Calculator")){if(Character.isDigit(k.charAt(0)))calc+=k.charAt(0);else if(k.equals("*"))calc+="+";else if(k.equals("Call"))calc+="-";else if(k.equals("Menu")){calc="";result="";}else if(k.equals("#")){try{long total=0;for(String n:calc.replace("-","+-").split("\\+"))if(!n.isEmpty())total+=Long.parseLong(n);result="= "+total;}catch(Exception e){result="Error";}}}}
 }
}
