package com.zamn.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.KeyEvent;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Locale;

/** R3: Android frontend for the unchanged Xbox ZNP4/ZDIR1/ZAUTH1/ZCHAT1/ZLB2 service. */
public class SetupActivity extends Activity {
    private static final int PICK_ROM=1001;
    private static final String SERVER="172.233.145.244";
    private static final int DIRECTORY_PORT=6467;
    private static final long BUILD=0x5A4D490AL;
    private final SecureRandom random=new SecureRandom();
    private File romPath;
    private XboxMenuView menu;
    private boolean roomListVisible;
    private boolean pauseOnly=false;
    private long leaderboardRequestSerial=0;
    private TextView status;
    private EditText ip, username, password, chatInput;
    private int lbScope=0,lbRules=0,lbLevel=0;
    private static final String[] LB_SCOPES={"SOLO ONLINE","CO-OP ONLINE"};
    private static final String[] LB_RULES={"NORMAL / SAVABLE","NORMAL / NO SAVE","HARDCORE / SAVABLE","HARDCORE / NO SAVE"};
    private static final String[] LB_LEVELS=new String[56];
    static {for(int i=0;i<LB_LEVELS.length;i++)LB_LEVELS[i]=String.format(Locale.US,"LEVEL %02d",i+1);}
    private String currentRoom="";
    private String currentPage="";
    private String storedUser="", storedToken="";
    private long romHash=0;
    private TouchSettings touchSettings;
    private GamepadSettings gamepadSettings;
    private int capturingAction=-1;
    private int netDelayChoice=-1;  // AUTO, otherwise 0..12 frame coverage
    private int netRollbackChoice=-1; // AUTO, OFF, 1, 2, 3 (host-only)
    private String netSettingsReturn="ONLINE";
    private static final String[] DELAY_CHOICES={"AUTO","0 FRAMES","1 FRAME","2 FRAMES","3 FRAMES","4 FRAMES","5 FRAMES","6 FRAMES","7 FRAMES","8 FRAMES","9 FRAMES","10 FRAMES","11 FRAMES","12 FRAMES"};
    private static final String[] RB_CHOICES={"AUTO","OFF","1 FRAME","2 FRAMES (MOBILE)","3 FRAMES (MOBILE)"};

    @Override public void onCreate(Bundle b){super.onCreate(b);File base=getExternalFilesDir(null);
        if(base==null){TextView t=new TextView(this);t.setText("App storage not available");setContentView(t);return;}
        romPath=new File(base,"zamn.sfc");
        SharedPreferences p=getPreferences(MODE_PRIVATE);
        storedUser=p.getString("profile_name","");storedToken=p.getString("profile_token","");
        touchSettings=new TouchSettings(this);
        gamepadSettings=new GamepadSettings(this);
        // R13: always start a newly launched app in AUTO. Prior manual
        // settings (including a lingering 11F delay) must not override AUTO.
        // Changing them in Network Options still affects the current session.
        netDelayChoice=-1;
        netRollbackChoice=-1;
        lbScope=Math.max(0,Math.min(LB_SCOPES.length-1,p.getInt("lb_scope",0)));
        lbRules=Math.max(0,Math.min(LB_RULES.length-1,p.getInt("lb_rules",0)));
        lbLevel=Math.max(0,Math.min(LB_LEVELS.length-1,p.getInt("lb_level",0)));
        pauseOnly=getIntent()!=null && getIntent().getBooleanExtra("pause_settings",false);
        if(pauseOnly){
            if(getIntent().getIntExtra("pause_settings_mode",1)==2)controllerMappingMenu();
            else openTouchEditor();
        }else buildUi();
    }
    @Override protected void onResume(){super.onResume();
        if(menu!=null){menu.setBackdrop(getExternalFilesDir(null));menu.invalidate();}
        enterImmersiveMenus();
    }
    @Override public void onWindowFocusChanged(boolean hasFocus){
        super.onWindowFocusChanged(hasFocus);
        if(hasFocus)enterImmersiveMenus();
    }
    private void enterImmersiveMenus(){
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_FULLSCREEN|
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private void showStatus(String s){runOnUiThread(()->{if(menu!=null)menu.setStatus(s);});}
    private XboxMenuView.Entry menuButton(String label,View.OnClickListener click){return menu.add(label,()->click.onClick(menu));}
    private EditText edit(String hint,String value){
        EditText data=new EditText(this);data.setSingleLine(true);data.setText(value);
        XboxMenuView.Entry row=menu.add(hint+": "+(value.isEmpty()?"[EDIT]":value),()->{
            EditText input=new EditText(this);input.setSingleLine(true);input.setText(data.getText());
            input.setInputType(data.getInputType());
            if(data.getInputType()==(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD))input.setInputType(data.getInputType());
            new AlertDialog.Builder(this).setTitle(hint).setView(input)
                .setPositiveButton("OK",(d,w)->{
                    data.setText(input.getText());
                    boolean secret=(data.getInputType()&InputType.TYPE_TEXT_VARIATION_PASSWORD)!=0;
                    menu.setEntryLabel(menuRowForEdit(data),hint+": "+(secret?"********":input.getText().toString()));
                }).setNegativeButton("CANCEL",null).show();
        });
        editRowLinks.put(data,row);
        return data;
    }
    private final java.util.IdentityHashMap<EditText,XboxMenuView.Entry> editRowLinks=new java.util.IdentityHashMap<>();
    private XboxMenuView.Entry menuRowForEdit(EditText data){return editRowLinks.get(data);}
    private boolean validRom(){return romPath.isFile()&&romPath.length()>=1024*1024&&romPath.length()<=12*1024*1024;}
    private void screen(String heading,String note){
        currentPage=heading;editRowLinks.clear();
        enterImmersiveMenus();
        menu=new XboxMenuView(this);menu.setBackdrop(getExternalFilesDir(null));
        menu.reset(heading,note);menu.setBackAction(()->onBackPressed());setContentView(menu);
        menu.requestFocus();
    }
    private void menuRow(String[] labels,View.OnClickListener[] actions){
        for(int i=0;i<labels.length;i++){final View.OnClickListener cb=actions[i];menu.add(labels[i],()->cb.onClick(menu));}
    }
    private void heading(String text){menu.addInfo(text);}
    private boolean ensureRom(){if(validRom())return true;mainMenu();showStatus("Import your own game ROM first.");return false;}
    private void buildUi(){mainMenu();}
    private void mainMenu(){
        screen("UNDEAD NEIGHBOR EATERS","ANDROID R13 / AUTO NETPLAY / XBOX CROSSPLAY");
        if(!validRom()){
            showStatus("Import your own ZAMN ROM to play. No game files are bundled.");
            menuButton("IMPORT GAME ROM",v->chooseRom());return;
        }
        romHash=hashRom();
        menuRow(new String[]{"OFFLINE", "ONLINE", "OPTIONS", "GAME MANUAL"},new View.OnClickListener[]{
            v->startGame("offline","",""),v->onlineMenu(),v->optionsMenu(),v->manualMenu()});
        showStatus("OFFLINE STARTS GAME / ONLINE / OPTIONS / MANUAL");
    }
    private void optionsMenu(){
        screen("OPTIONS","MOBILE TOUCH CONTROLS / PERSONALIZATION");
        menuButton("MOVE / RESIZE TOUCH CONTROLS",v->openTouchEditor());
        menuButton("TOUCH HUD: "+(touchSettings.enabled?"VISIBLE":"HIDDEN"),v->{
            touchSettings.enabled=!touchSettings.enabled;touchSettings.write();optionsMenu();});
        menuButton("AUTO HIDE WITH GAMEPAD: "+(touchSettings.autoHide?"ON":"OFF"),v->{
            touchSettings.autoHide=!touchSettings.autoHide;touchSettings.write();optionsMenu();});
        menuButton("D-PAD: "+new String[]{"CROSS (8-WAY)","EIGHT-WAY DISC","VIRTUAL STICK"}[touchSettings.dpadStyle],v->{
            touchSettings.dpadStyle=(touchSettings.dpadStyle+1)%3;touchSettings.write();optionsMenu();});
        menuButton("D-PAD POSITION: "+(touchSettings.floating?"FLOATING":"LOCKED"),v->{
            touchSettings.floating=!touchSettings.floating;touchSettings.write();optionsMenu();});
        menuButton("D-PAD DEADZONE: "+Math.round(touchSettings.deadzone*100)+"%",v->dpadDeadzoneMenu());
        menuButton("REMAP PHYSICAL CONTROLLER",v->controllerMappingMenu());
        menuButton("LOCAL SAVE PLAYER: "+getPreferences(MODE_PRIVATE).getString("save_player",storedUser.isEmpty()?"LOCAL":storedUser),v->chooseSavePlayer());
        menuButton("RESET TOUCH LAYOUT",v->{
            new AlertDialog.Builder(this).setTitle("Reset touch controls?")
                .setMessage("Restore the default button positions and sizes?")
                .setPositiveButton("RESET",(d,w)->{touchSettings.reset();optionsMenu();})
                .setNegativeButton("CANCEL",null).show();});
        menuButton("BACK",v->mainMenu());
        showStatus("CHANGES SAVE AUTOMATICALLY. RESTART GAME TO APPLY.");
    }
    private void chooseSavePlayer(){
        EditText name=new EditText(this);name.setSingleLine(true);
        name.setText(getPreferences(MODE_PRIVATE).getString("save_player",storedUser.isEmpty()?"LOCAL":storedUser));
        name.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(12)});
        new AlertDialog.Builder(this).setTitle("Local save player")
          .setMessage("Saves are separated per local player and game mode. Use 1-12 letters or numbers.")
          .setView(name).setPositiveButton("SAVE",(d,w)->{
            String v=name.getText().toString().trim().toUpperCase(Locale.US);
            if(v.matches("[A-Z0-9_-]{1,12}")){
                getPreferences(MODE_PRIVATE).edit().putString("save_player",v).apply();optionsMenu();
            }else showStatus("INVALID PLAYER: USE 1-12 LETTERS OR NUMBERS");
          }).setNegativeButton("CANCEL",null).show();
    }
    private void controllerMappingMenu(){
        screen("CONTROLLER MAPPING","TAP AN ACTION THEN PRESS YOUR GAMEPAD BUTTON");
        for(int i=0;i<GamepadSettings.ACTIONS.length;i++){
            final int action=i;
            menuButton(GamepadSettings.ACTIONS[i]+"  :  "+GamepadSettings.describe(gamepadSettings.bindings[i]),v->{
                capturingAction=action;
                showStatus("PRESS A BUTTON ON YOUR CONNECTED GAMEPAD / BACK TO CANCEL");
            });
        }
        menuButton("RESTORE DEFAULT GAMEPAD MAPPING",v->{gamepadSettings.reset();controllerMappingMenu();});
        menuButton("BACK",v->{if(pauseOnly)finish();else optionsMenu();});
    }
    private void dpadDeadzoneMenu(){
        screen("D-PAD DEADZONE","SELECT HOW FAR THE THUMB MOVES BEFORE INPUT STARTS");
        String[] labels={"12% / VERY RESPONSIVE","18% / PRECISE","25% / BALANCED","35% / STEADY","45% / LARGE DEADZONE"};
        float[] amounts={.12f,.18f,.25f,.35f,.45f};
        for(int i=0;i<labels.length;i++){final int idx=i;
            menuButton(labels[i],v->{touchSettings.deadzone=amounts[idx];touchSettings.write();optionsMenu();});}
        menuButton("BACK",v->optionsMenu());
    }
    private void netplayPicker(boolean delay){
        String[] choices=delay?DELAY_CHOICES:RB_CHOICES;
        screen(delay?"SELECT DELAY":"SELECT ROLLBACK", "AUTO RECOMMENDED / TAP OPTION TO SELECT");
        for(int i=0;i<choices.length;i++){final int idx=i;
            menuButton(choices[i],v->{
                if(delay){netDelayChoice=idx-1;}
                else{netRollbackChoice=idx-1;}
                netplaySettingsMenu();
            });
        }
        menuButton("BACK",v->netplaySettingsMenu());
    }
    private void showNetworkSettingsFrom(String page){netSettingsReturn=page;netplaySettingsMenu();}
    private void returnFromNetworkSettings(){
        switch(netSettingsReturn){
            case "DIRECT":directMenu();break;
            case "PUBLIC":publicMenu();break;
            case "HOST_DIRECT":hostDirectMenu();break;
            case "HOST_PUBLIC":hostPublicMenu();break;
            default:onlineMenu();break;
        }
    }
    private String hostSettingSummary(){
        return "DELAY "+(netDelayChoice<0?"AUTO":("MANUAL "+netDelayChoice+"F"))+
               " / RB "+(netRollbackChoice<0?"AUTO":("MANUAL "+netRollbackChoice+"F"));
    }
    private void netplaySettingsMenu(){
        screen("NETPLAY OPTIONS","AUTO DEFAULTS TO ROLLBACK ON / MANUAL OVERRIDES AVAILABLE");
        menu.addInfo("CURRENT: "+hostSettingSummary());
        menuButton("RESTORE BOTH TO AUTO",v->{netDelayChoice=-1;netRollbackChoice=-1;
            netplaySettingsMenu();});
        menu.addChoice("INPUT DELAY",DELAY_CHOICES,netDelayChoice+1,newIndex->{
            netDelayChoice=newIndex-1;netplaySettingsMenu();
        },()->netplayPicker(true));
        menu.addChoice("ROLLBACK",RB_CHOICES,netRollbackChoice+1,newIndex->{
            netRollbackChoice=newIndex-1;netplaySettingsMenu();
        },()->netplayPicker(false));
        menu.addInfo("XBOX PEER: MAX 1 ROLLBACK FRAME");
        menu.addInfo("ANDROID PAIR: UP TO 3 ROLLBACK FRAMES");
        menu.addInfo("AUTO: 1-3F RB FOR ANDROID PAIRS");
        menu.addInfo("AUTO: 1F MAXIMUM FOR XBOX GUESTS");
        menu.addInfo("LOBBY: RB MODE CYCLES AUTO/OFF/1/2/3");
        menuButton("BACK",v->returnFromNetworkSettings());
    }
    private void openTouchEditor(){
        currentPage="CONTROL LAYOUT";
        TouchLayoutView editor=new TouchLayoutView(this,touchSettings,()->{if(pauseOnly)finish();else optionsMenu();});
        setContentView(editor);editor.requestFocus();
    }
    private void onlineMenu(){
        screen("ONLINE", "XBOX NETPLAY PROTOCOL 4  •  BUILD 5A4D490A");
        menuRow(new String[]{"PUBLIC ROOMS", "DIRECT CONNECT"},new View.OnClickListener[]{
            v->publicMenu(),v->directMenu()});
        menuRow(new String[]{"SOLO PLAY", "LEADERBOARDS", "PROFILE"},new View.OnClickListener[]{
            v->startGame("offline","",""),v->leaderboardMenu(),v->profileMenu()});
        menuButton("BACK",v->mainMenu());
    }
    private void directMenu(){
        screen("DIRECT CONNECT", "JOIN AN EXISTING ORIGINAL XBOX HOST  •  UDP 6464");
        ip=edit("Xbox IP, e.g. 192.168.1.50",getPreferences(MODE_PRIVATE).getString("xbox_ip",""));
        menuButton("JOIN DIRECT",v->{String host=ip.getText().toString().trim();
            if(!host.matches("[0-9.]{7,15}")){showStatus("Enter a numeric Xbox IPv4 address.");return;}
            getPreferences(MODE_PRIVATE).edit().putString("xbox_ip",host).apply();startGame("direct",host,"");});
        menuButton("HOST DIRECT (XBOX-STYLE LOBBY)",v->hostDirectMenu());
        menuButton("NETWORK AUTO / MANUAL",v->showNetworkSettingsFrom("DIRECT"));
        menuButton("BACK",v->onlineMenu());
    }
    private void publicMenu(){
        screen("PUBLIC ROOMS","EXISTING XBOX DIRECTORY  •  UDP 6467");
        menuButton("JOIN PUBLIC ROOM",v->publicList());
        menuButton("WORLD CHAT",v->chatMenu(""));
        menuButton("HOST PUBLIC ROOM",v->hostPublicMenu());
        menuButton("NETWORK AUTO / MANUAL",v->showNetworkSettingsFrom("PUBLIC"));
        menuButton("BACK",v->onlineMenu());
    }
    private static String localLanIp(){
        try{
            java.util.Enumeration<java.net.NetworkInterface> nets=java.net.NetworkInterface.getNetworkInterfaces();
            while(nets.hasMoreElements()){
                java.net.NetworkInterface net=nets.nextElement();
                if(!net.isUp()||net.isLoopback())continue;
                java.util.Enumeration<java.net.InetAddress> addresses=net.getInetAddresses();
                while(addresses.hasMoreElements()){
                    java.net.InetAddress a=addresses.nextElement();
                    if(a instanceof java.net.Inet4Address && a.isSiteLocalAddress())return a.getHostAddress();
                }
            }
        }catch(Exception ignored){}
        return "CHECK YOUR WI-FI SETTINGS";
    }
    private void hostDirectMenu(){
        screen("HOST DIRECT","ANDROID HOST P1 / PRESS A WHEN P2 READY");
        menu.addInfo("PHONE LAN IP: "+localLanIp());
        menu.addInfo("UDP 6464 / OTHER PLAYER USES JOIN DIRECT");
        menu.addInfo("KEEP THE APP OPEN WHILE WAITING FOR GUEST");
        menu.addInfo("NETWORK: "+hostSettingSummary());
        menuButton("START HOSTING",v->startGame("host_direct","",""));
        menuButton("NETWORK AUTO / MANUAL",v->showNetworkSettingsFrom("HOST_DIRECT"));
        menuButton("BACK",v->directMenu());
    }
    private void hostPublicMenu(){
        screen("HOST PUBLIC ROOM","USE THE EXISTING XBOX ROOM DIRECTORY");
        menu.addInfo("PROFILE: "+(storedUser.isEmpty()?"NOT SIGNED IN":storedUser));
        menu.addInfo("ANDROID ROOM / XBOX GUESTS SUPPORTED");
        menu.addInfo("HOST PRESSES A WHEN PLAYER 2 READY");
        menu.addInfo("NETWORK: "+hostSettingSummary());
        menuButton("REGISTER AND HOST",v->startGame("host_public","",""));
        menuButton("NETWORK AUTO / MANUAL",v->showNetworkSettingsFrom("HOST_PUBLIC"));
        menuButton("BACK",v->publicMenu());
    }
    private void publicList(){
        screen("JOIN PUBLIC ROOM", "PUBLIC ROOMS ON THE UNMODIFIED XBOX SERVER");
        menuButton("REFRESH ROOMS",v->listRooms());
        menuButton("WORLD CHAT",v->chatMenu(""));
        roomListVisible=true;
        menuButton("BACK",v->publicMenu());
        listRooms();
    }
    private void profileMenu(){
        screen("PROFILE",storedUser.length()>0?"SIGNED IN AS "+storedUser:"SIGN IN OR CREATE AN ACCOUNT");
        username=edit("Profile name (3–12 characters)",storedUser);
        password=edit("Password (4–16 characters; not saved)","");
        password.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD);
        menuButton("SIGN IN / CHECK TOKEN",v->authenticate(false,()->showStatus("SIGNED IN: "+storedUser)));
        menuButton("CREATE PROFILE",v->authenticate(true,()->showStatus("PROFILE CREATED: "+storedUser)));
        menuButton("BACK",v->onlineMenu());
    }
    private void saveLeaderboardSelection(){
        getPreferences(MODE_PRIVATE).edit().putInt("lb_scope",lbScope)
            .putInt("lb_rules",lbRules).putInt("lb_level",lbLevel).apply();
    }
    private void leaderboardMenu(){
        screen("LEADERBOARDS","XBOX LEADERBOARDS / TOUCH < > TO CHANGE");
        menu.addChoice("PLAYERS",LB_SCOPES,lbScope,n->{lbScope=n;saveLeaderboardSelection();},
            ()->leaderboardPicker("CHOOSE PLAYERS",LB_SCOPES,lbScope,0));
        menu.addChoice("RULESET",LB_RULES,lbRules,n->{lbRules=n;saveLeaderboardSelection();},
            ()->leaderboardPicker("CHOOSE RULESET",LB_RULES,lbRules,1));
        menu.addChoice("LEVEL",LB_LEVELS,lbLevel,n->{lbLevel=n;saveLeaderboardSelection();},
            ()->leaderboardPicker("CHOOSE LEVEL",LB_LEVELS,lbLevel,2));
        menuButton("FETCH TOP TIMES",v->fetchLeaderboard());
        menuButton("BACK",v->onlineMenu());
        showStatus("TAP < > OR SWIPE SIDEWAYS / TAP CENTER FOR FULL LIST");
    }
    private void leaderboardPicker(String heading,String[] choices,int current,int field){
        screen(heading,"TAP AN OPTION / SWIPE UP OR DOWN");
        for(int i=0;i<choices.length;i++){
            final int selectedValue=i;
            menuButton((i==current?"> ":"  ")+choices[i],v->{
                if(field==0)lbScope=selectedValue;
                else if(field==1)lbRules=selectedValue;
                else lbLevel=selectedValue;
                saveLeaderboardSelection();leaderboardMenu();
            });
        }
        menuButton("BACK",v->leaderboardMenu());
        menu.focusEntry(current);
    }
    private void fetchLeaderboard(){
        final long serial=++leaderboardRequestSerial;
        if(storedToken.length()==0){showStatus("Sign in under PROFILE before viewing the online leaderboard.");return;}
        int scope=lbScope,rules=lbRules;
        int level=lbLevel+1;
        final String resultHeading=LB_RULES[rules]+" / "+LB_LEVELS[lbLevel];
        long nonce=(random.nextInt()&0x7fffffffL)+1L;
        String q=String.format(Locale.US,"ZLB2|LISTD|%08X|%016X|%s|%d|%d|%d|%d",BUILD,romHash,storedToken,scope,rules,level,nonce);
        showStatus("Fetching leaderboard...");
        async(()->{
            String reply=udpQuery(q,"ZLB2|",3500);
            if(reply.startsWith("ZLB2|ERR|"))return reply;
            String[] p=reply.split("\\|",-1);
            if(p.length<4||(!p[1].equals("PACKD")&&!p[1].equals("PACK"))||!p[2].equals(Long.toString(nonce)))return "Leaderboard response: "+reply;
            int count=Integer.parseInt(p[3]);int stride=p[1].equals("PACKD")?4:3;
            StringBuilder out=new StringBuilder();
            for(int i=0;i<count&&i<5;i++){
                int j=4+i*stride;if(j+stride>p.length)break;
                long frames=Long.parseLong(p[j]);
                // A record is frames | players | profile/team | date (PACKD).
                // Keep delimiters out of untrusted fields; never alter wire format.
                String name=p[j+2].replace('\n',' ').replace('\r',' ').replace('\t',' ');
                String date=stride==4?p[j+3].replace('\n',' ').replace('\r',' '):"--";
                out.append(i+1).append("|" ).append(name.replace('|','/')).append("|")
                   .append(LeaderboardTime.fromFrames(frames)).append("|").append(date.replace('|','/')).append("\n");
            }
            return out.toString();
        },result->{
            if(serial!=leaderboardRequestSerial ||
               (!"LEADERBOARDS".equals(currentPage)&&!"TOP TIMES".equals(currentPage)))return;
            screen("TOP TIMES",resultHeading);
            menu.setHorizontalSwipeAction(delta->{
                lbLevel=(lbLevel+delta+LB_LEVELS.length)%LB_LEVELS.length;
                saveLeaderboardSelection();fetchLeaderboard();
            });
            if(result.startsWith("ZLB2|ERR|")||result.startsWith("Leaderboard response:")||result.startsWith("ERROR:")||result.equals("TIMEOUT")){
                menu.addInfo(result);
            }else if(result.isEmpty()){
                menu.addInfo("NO ENTRIES");
            }else{
                for(String line:result.split("\n")){
                    String[] row=line.split("\\|",4);
                    if(row.length!=4)continue;
                    try{menu.addScore(Integer.parseInt(row[0]),row[1],row[2],row[3]);}
                    catch(NumberFormatException ignored){}
                }
            }
            menuButton("BACK",v->leaderboardMenu());
            showStatus("SWIPE LEFT / RIGHT FOR NEXT / PREVIOUS LEVEL");
        });
    }
    private void chatMenu(String room){
        currentRoom=room;
        screen(room.length()>0?"LOBBY CHAT":"WORLD CHAT", "XBOX WORLD / LOBBY CHAT  •  ZCHAT1");
        if(storedToken.length()==0)showStatus("Sign in under PROFILE first.");
        menuButton("REFRESH MESSAGES",v->refreshChat());
        chatInput=edit("Type message (max 48 characters)","");
        menuButton("SEND MESSAGE",v->sendChat());
        menuButton("BACK",v->{if(currentRoom.length()>0)publicList();else publicMenu();});
        if(storedToken.length()>0)refreshChat();
    }
    private String channel(){return currentRoom.length()>0?"LOBBY":"WORLD";}
    private String chatRoom(){return currentRoom.length()>0?currentRoom:"-";}
    private void refreshChat(){
        if(storedToken.length()==0){showStatus("Sign in under PROFILE first.");return;}
        String channel=channel(),room=chatRoom(),token=storedToken;
        showStatus("Loading "+channel+" chat...");
        async(()->{
            byte[] q=("ZCHAT1|LIST|"+token+"|"+channel+"|"+room).getBytes(StandardCharsets.US_ASCII);
            InetAddress server=InetAddress.getByName(SERVER);StringBuilder lines=new StringBuilder(channel+" CHAT\n");
            try(DatagramSocket sock=new DatagramSocket()){
                sock.setSoTimeout(150);long start=System.currentTimeMillis(),sent=0;int count=0;
                while(System.currentTimeMillis()-start<2500){
                    if(System.currentTimeMillis()-sent>=380){sock.send(new DatagramPacket(q,q.length,server,DIRECTORY_PORT));sent=System.currentTimeMillis();}
                    byte[] bytes=new byte[512];DatagramPacket p=new DatagramPacket(bytes,bytes.length);
                    try{sock.receive(p);}catch(java.net.SocketTimeoutException ex){continue;}
                    if(!p.getAddress().equals(server)||p.getPort()!=DIRECTORY_PORT)continue;
                    String response=new String(p.getData(),0,p.getLength(),StandardCharsets.US_ASCII);
                    if(response.startsWith("ZCHAT1|BEGIN|")){lines=new StringBuilder(channel+" CHAT\n");count=0;}
                    else if(response.startsWith("ZCHAT1|ROW|")){String[] parts=response.split("\\|",5);
                        if(parts.length>=5){lines.append(parts[3]).append(": ").append(parts[4]).append("\n");count++;}}
                    else if(response.equals("ZCHAT1|END"))return count==0?"NO MESSAGES":lines.toString();
                    else if(response.startsWith("ZCHAT1|ERR|"))return response;
                }
            }
            return lines.toString();
        },result->{
            if(!"WORLD CHAT".equals(currentPage) && !"LOBBY CHAT".equals(currentPage))return;
            menu.clearInfo();
            for(String line:result.split("\n"))if(!line.trim().isEmpty())menu.addInfo(line);
        });
    }
    private void sendChat(){
        if(storedToken.isEmpty()){showStatus("Sign in first.");return;}
        String message=chatInput.getText().toString().trim();
        if(message.length()==0||message.length()>48||message.contains("|")){showStatus("Message must contain 1–48 characters, no | separators.");return;}
        String query="ZCHAT1|SEND|"+storedToken+"|"+channel()+"|"+chatRoom()+"|"+message;
        showStatus("Sending message...");
        async(()->udpQuery(query,"ZCHAT1|",2800),reply->{if(reply.startsWith("ZCHAT1|OK|SEND"))refreshChat();});
    }
    private void manualMenu(){
        screen("GAME MANUAL","ANDROID R5 / CONTROL REFERENCE");
        heading("MOVEMENT: D-PAD / LEFT STICK / TOUCH PAD");
        heading("A: CYCLE WEAPON  •  B: CHANGE ITEM  •  X: SHOOT");
        heading("R1 / BLACK: MAP / RADAR");
        heading("LT / RT: WEAPON CONTROLS (GAMEPLAY MAPPING)");
        menuButton("BACK",v->mainMenu());
        showStatus("The Xbox image-based manual viewer isn't ported yet; these are control hints, not a replacement for the original manual.");
    }
    @Override public boolean onKeyDown(int key,KeyEvent event){
        if(capturingAction>=0){
            if(key==KeyEvent.KEYCODE_BACK){capturingAction=-1;controllerMappingMenu();return true;}
            int input=GamepadSettings.androidToSdl(key);
            if(input>=0){gamepadSettings.bind(capturingAction,input);capturingAction=-1;controllerMappingMenu();return true;}
            showStatus("UNRECOGNIZED BUTTON / TRY A B X Y LB RB LT RT START SELECT");return true;
        }
        if(key==KeyEvent.KEYCODE_BUTTON_B || key==KeyEvent.KEYCODE_BACK){onBackPressed();return true;}
        if(menu!=null){
            if(key==KeyEvent.KEYCODE_DPAD_LEFT || key==KeyEvent.KEYCODE_BUTTON_L1){
                if(!menu.adjustSelected(-1))menu.navigate(-1);return true;
            }
            if(key==KeyEvent.KEYCODE_DPAD_RIGHT || key==KeyEvent.KEYCODE_BUTTON_R1){
                if(!menu.adjustSelected(1))menu.navigate(1);return true;
            }
            if(key==KeyEvent.KEYCODE_DPAD_UP){menu.navigateVertical(-1);return true;}
            if(key==KeyEvent.KEYCODE_DPAD_DOWN){menu.navigateVertical(1);return true;}
            if(key==KeyEvent.KEYCODE_BUTTON_A || key==KeyEvent.KEYCODE_BUTTON_START ||
               key==KeyEvent.KEYCODE_ENTER || key==KeyEvent.KEYCODE_DPAD_CENTER){menu.activate();return true;}
        }
        return super.onKeyDown(key,event);
    }
    @Override public void onBackPressed(){
        if(pauseOnly && (currentPage.equals("CONTROL LAYOUT")||currentPage.equals("CONTROLLER MAPPING")||currentPage.equals("OPTIONS"))){touchSettings.write();finish();return;}
        if(currentPage.equals("ONLINE")||currentPage.equals("OPTIONS")){mainMenu();return;}
        if(currentPage.equals("CONTROL LAYOUT")){touchSettings.write();optionsMenu();return;}
        if(currentPage.equals("CONTROLLER MAPPING")){capturingAction=-1;optionsMenu();return;}
        if(currentPage.equals("NETPLAY OPTIONS")){returnFromNetworkSettings();return;}
        if(currentPage.equals("SELECT DELAY")||currentPage.equals("SELECT ROLLBACK")){netplaySettingsMenu();return;}
        if(currentPage.equals("D-PAD DEADZONE")){optionsMenu();return;}
        if(currentPage.equals("PUBLIC ROOMS")||currentPage.equals("DIRECT CONNECT") ||
           currentPage.equals("LEADERBOARDS")||currentPage.equals("PROFILE")){onlineMenu();return;}
        if(currentPage.equals("JOIN PUBLIC ROOM")){publicMenu();return;}
        if(currentPage.equals("HOST DIRECT")){directMenu();return;}
        if(currentPage.equals("HOST PUBLIC ROOM")){publicMenu();return;}
        if(currentPage.equals("WORLD CHAT")){publicMenu();return;}
        if(currentPage.equals("LOBBY CHAT")){publicList();return;}
        if(currentPage.equals("GAME MANUAL")){mainMenu();return;}
        if(currentPage.equals("TOP TIMES")||currentPage.startsWith("CHOOSE ")){leaderboardMenu();return;}
        super.onBackPressed();
    }
    private void chooseRom(){Intent pick=new Intent(Intent.ACTION_OPEN_DOCUMENT);pick.addCategory(Intent.CATEGORY_OPENABLE);pick.setType("*/*");startActivityForResult(pick,PICK_ROM);}
    @Override protected void onActivityResult(int req,int result,Intent data){super.onActivityResult(req,result,data);
        if(req!=PICK_ROM||result!=RESULT_OK||data==null||data.getData()==null)return;
        Uri uri=data.getData();File temp=new File(romPath.getParentFile(),"zamn.sfc.part");long total=0;
        try(InputStream in=getContentResolver().openInputStream(uri);FileOutputStream out=new FileOutputStream(temp)){
            if(in==null)throw new IOException("Cannot open ROM");byte[] buf=new byte[65536];int n;
            while((n=in.read(buf))!=-1){total+=n;if(total>12L*1024*1024)throw new IOException("ROM exceeds size limit");out.write(buf,0,n);}
            if(total<1024*1024)throw new IOException("Selected ROM is too small");
            if(romPath.exists()&&!romPath.delete())throw new IOException("Cannot replace ROM");
            if(!temp.renameTo(romPath))throw new IOException("Could not finish ROM import");buildUi();
        }catch(Exception e){temp.delete();showStatus("ROM import failed: "+e.getMessage());}
    }
    /* Exactly the custom FNV64 seed/prime in xbox_main.cpp, including its seed. */
    private static long fnv(long seed,byte[]data){long h=seed==0?0xcbf29ce484222325L:seed;for(byte b:data){h^=b&0xffL;h*=1099511628211L;}return h;}
    private long hashRom(){long h=1469598103934665603L;try(FileInputStream in=new FileInputStream(romPath)){byte[] buf=new byte[65536];int n;while((n=in.read(buf))!=-1){for(int i=0;i<n;i++){h^=(buf[i]&255L);h*=1099511628211L;}}}catch(Exception e){showStatus("ROM hash failed: "+e.getMessage());return 0;}return h;}
    private String id(){SharedPreferences p=getPreferences(MODE_PRIVATE);String v=p.getString("android_id","");if(v.matches("[0-9A-F]{16}"))return v;
        v=String.format(Locale.US,"%016X",random.nextLong());p.edit().putString("android_id",v).apply();return v;
    }
    private static String hex32(long v){return String.format(Locale.US,"%08X%08X",(v>>>32)&0xffffffffL,v&0xffffffffL);}
    private static String proof(String user,String pass){
        byte[]initial=("ZAMN-R499:"+user+":"+pass).getBytes(StandardCharsets.US_ASCII);
        long a=fnv(0x9e3779b97f4a7c15L,initial);
        long b=fnv(0xd6e8feb86659fd93L^0xa5a5a5a5a5a5a5a5L,initial);
        ByteBuffer bb=ByteBuffer.allocate(20).order(ByteOrder.LITTLE_ENDIAN);
        for(int i=0;i<4096;++i){bb.clear();bb.putLong(a);bb.putLong(b);bb.putInt(i);byte[]mix=bb.array();
            a=fnv(a^b,mix);b=fnv(b^a^0x517cc1b727220a95L,mix);
        }
        return hex32(a)+hex32(b);
    }
    private interface StringWorker { String run() throws Exception; }
    private interface Done { void accept(String s); }
    private void async(StringWorker work,Done done){
        new Thread(()->{String s;try{s=work.run();}catch(Exception e){s="ERROR: "+e.getMessage();Log.e("ZAMN-R2",s,e);}final String v=s;runOnUiThread(()->{if(menu!=null)menu.setStatus(v);if(done!=null)done.accept(v);});},"ZAMN-Network").start();
    }
    private static String udpQuery(String text,String prefix,int timeout) throws Exception{
        byte[] bytes=text.getBytes(StandardCharsets.US_ASCII);
        try(DatagramSocket sock=new DatagramSocket()){
            sock.setSoTimeout(200);InetAddress server=InetAddress.getByName(SERVER);long until=System.currentTimeMillis()+timeout;long sent=0;
            while(System.currentTimeMillis()<until){
                if(System.currentTimeMillis()-sent>=350){sock.send(new DatagramPacket(bytes,bytes.length,server,DIRECTORY_PORT));sent=System.currentTimeMillis();}
                byte[] response=new byte[1024];DatagramPacket packet=new DatagramPacket(response,response.length);
                try{sock.receive(packet);}catch(java.net.SocketTimeoutException e){continue;}
                if(!packet.getAddress().equals(server)||packet.getPort()!=DIRECTORY_PORT)continue;
                String s=new String(packet.getData(),packet.getOffset(),packet.getLength(),StandardCharsets.US_ASCII);
                if(s.startsWith(prefix))return s;
            }
            return "TIMEOUT";
        }
    }
    private void authenticate(boolean create,Runnable success){
        String user=username.getText().toString().trim().toUpperCase(Locale.US);
        String pass=password.getText().toString();
        if(!user.matches("[A-Z0-9_-]{3,12}")){showStatus("Use 3–12 uppercase letters/numbers/_/- for profile name.");return;}
        boolean tokenOnly=!create && pass.isEmpty() && storedToken.length()>0 && user.equals(storedUser);
        if(!tokenOnly && (pass.length()<4 || pass.length()>16)){showStatus("Enter a 4–16 character password, or use a stored sign-in token.");return;}
        showStatus("Contacting existing ZAMN profile service...");
        async(()->{
            String msg=tokenOnly ? String.format(Locale.US,"ZAUTH1|TOKEN|%08X|%016X|%s|%s|%s",BUILD,romHash,user,storedToken,id())
              : String.format(Locale.US,"ZAUTH1|%s|%08X|%016X|%s|%s|%s",create?"CREATE":"LOGIN",BUILD,romHash,user,proof(user,pass),id());
            String r=udpQuery(msg,"ZAUTH1|",3500);
            if(!r.startsWith("ZAUTH1|OK|"))return "PROFILE AUTH: "+r;
            String[] a=r.split("\\|",-1);if(a.length<4||!a[3].matches("[A-Za-z0-9_-]{1,32}"))return "Invalid server authentication response";
            storedUser=a[2];storedToken=a[3];
            getPreferences(MODE_PRIVATE).edit().putString("profile_name",storedUser).putString("profile_token",storedToken).apply();
            return "PROFILE READY: "+storedUser;
        },r->{if(r.startsWith("PROFILE READY"))success.run();});
    }
    private void listRooms(){
        showStatus("Fetching public rooms from the existing Xbox server...");
        async(()->{
            String q=String.format(Locale.US,"ZDIR1|LIST|%08X|%016X",BUILD,romHash);
            LinkedHashMap<String,String> found=new LinkedHashMap<>();byte[] b=q.getBytes(StandardCharsets.US_ASCII);
            try(DatagramSocket sock=new DatagramSocket()){
                sock.setSoTimeout(250);InetAddress server=InetAddress.getByName(SERVER);
                long start=System.currentTimeMillis();long last=0;boolean begun=false;
                while(System.currentTimeMillis()-start<2800){
                    long now=System.currentTimeMillis();if(now-last>=500){sock.send(new DatagramPacket(b,b.length,server,DIRECTORY_PORT));last=now;}
                    byte[] data=new byte[512];DatagramPacket p=new DatagramPacket(data,data.length);
                    try{sock.receive(p);}catch(java.net.SocketTimeoutException e){continue;}
                    if(!p.getAddress().equals(server)||p.getPort()!=DIRECTORY_PORT)continue;
                    String s=new String(data,0,p.getLength(),StandardCharsets.US_ASCII);
                    if(s.startsWith("ZDIR1|LISTBEGIN|")){begun=true;found.clear();}
                    else if(s.equals("ZDIR1|LISTEND")){break;}
                    else if(begun&&s.startsWith("ZDIR1|ROOM|")){
                        String[] a=s.split("\\|",-1);if(a.length>=7&&a[2].matches("[A-Za-z0-9_-]{1,11}"))found.put(a[2],a[3]+"  "+a[4]+"/"+a[5]+"  ruleset="+a[6]);
                    }
                }
            }
            ArrayList<String> entries=new ArrayList<>();for(String key:found.keySet())entries.add(key+"~"+found.get(key));
            runOnUiThread(()->showRooms(entries));return "Xbox public rooms found: "+entries.size();
        },null);
    }
    private void showRooms(ArrayList<String> entries){
        if(!roomListVisible || !"JOIN PUBLIC ROOM".equals(currentPage))return;
        screen("JOIN PUBLIC ROOM","XBOX PUBLIC ROOMS / BUILD 5A4D490A");
        menuButton("REFRESH ROOMS",v->listRooms());
        menuButton("WORLD CHAT",v->chatMenu(""));
        if(entries.isEmpty())menu.addInfo("NO OPEN ROOMS / REFRESH");
        for(String pair:entries){String[] t=pair.split("~",2);String room=t[0];
            menuButton("JOIN " +(t.length>1?t[1]:room),v->startGame("public","",room));
            menuButton("LOBBY CHAT "+room,v->chatMenu(room));
        }
        menuButton("BACK",v->publicMenu());
    }
    private void startGame(String mode,String host,String room){
        if(!validRom()){showStatus("Import a ROM first");return;}
        if((mode.equals("public")||mode.equals("host_public")) && storedToken.isEmpty()){showStatus("Sign in before joining public rooms");return;}
        try{
            touchSettings.write();
            File privateDir=getFilesDir();File cfg=new File(privateDir,"zamn_online.conf");
            File boot=new File(privateDir,"zamn_boot.cfg");
            File session=new File(privateDir,"zamn_session.cfg");
            String player=getPreferences(MODE_PRIVATE).getString("save_player",storedUser.isEmpty()?"LOCAL":storedUser);
            // Only local offline sessions use unsynchronized save states.
            String saveMode=mode.equals("offline")?"normal_savable":"online_no_local_save";
            try(FileOutputStream out=new FileOutputStream(session)){
                out.write(("player="+player+"\nmode="+saveMode+"\n").getBytes(StandardCharsets.US_ASCII));
            }
            try(FileOutputStream b=new FileOutputStream(boot)){b.write((mode.equals("password")?"password\n":"normal\n").getBytes(StandardCharsets.US_ASCII));}
            String config="mode="+(mode.equals("password")?"offline":mode)+"\nhost="+host+"\nroom="+room+"\nusername="+(storedUser.isEmpty()?"ANDROID":storedUser)+"\ntoken="+storedToken+"\ndelay="+netDelayChoice+"\nrollback="+netRollbackChoice+"\n";
            try(FileOutputStream out=new FileOutputStream(cfg)){out.write(config.getBytes(StandardCharsets.US_ASCII));}
            startActivity(new Intent(this,GameActivity.class));
        }catch(Exception e){showStatus("Could not start game: "+e.getMessage());}
    }
}
