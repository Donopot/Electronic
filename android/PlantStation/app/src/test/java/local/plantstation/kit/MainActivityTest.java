package local.plantstation.kit;
import android.Manifest;
import android.app.Application;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowWifiInfo;
import org.robolectric.shadows.ShadowNetwork;
import org.robolectric.shadows.ShadowNetworkCapabilities;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import com.espressif.provisioning.ESPDevice;
import com.espressif.provisioning.ESPConstants;
import com.espressif.provisioning.DeviceConnectionEvent;
import com.espressif.provisioning.listeners.ResponseListener;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import static org.junit.Assert.*;

/** Real Activity, simulated Android runtime. Physical permissions/radio remain
 * a separate phone acceptance check. No BLE device or network is contacted. */
@RunWith(RobolectricTestRunner.class) @Config(sdk=28) @LooperMode(LooperMode.Mode.PAUSED)
public class MainActivityTest {
    private static <T extends View> T find(View view,Class<T> type,String label){
        if(type.isInstance(view)&&(label==null||(view instanceof TextView&&((TextView)view).getText().toString().equals(label))))return type.cast(view);
        if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++){T child=find(group.getChildAt(i),type,label);if(child!=null)return child;}}
        return null;
    }
    private static String text(View view){String result=view instanceof TextView?((TextView)view).getText().toString():"";if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++)result+="\n"+text(group.getChildAt(i));}return result;}
    private static View root(MainActivity activity){return activity.findViewById(android.R.id.content);}
    private static EditText input(MainActivity activity,String label){
        TextView caption=find(root(activity),TextView.class,label);assertNotNull(label,caption);
        EditText field=activity.findViewById(caption.getLabelFor());assertNotNull("labelFor: "+label,field);return field;
    }
    private static Object get(MainActivity activity,String name){try{Field field=MainActivity.class.getDeclaredField(name);field.setAccessible(true);return field.get(activity);}catch(Exception e){throw new AssertionError(e);}}
    private static void set(MainActivity activity,String name,Object value){try{Field field=MainActivity.class.getDeclaredField(name);field.setAccessible(true);field.set(activity,value);}catch(Exception e){throw new AssertionError(e);}}
    private static void invoke(MainActivity activity,String name,Class<?>[] types,Object... args){try{Method method=MainActivity.class.getDeclaredMethod(name,types);method.setAccessible(true);method.invoke(activity,args);}catch(Exception e){throw new AssertionError(e);}}
    private static void idle(){Shadows.shadowOf(Looper.getMainLooper()).idle();}
    private static void nextPoll(){Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(750));}
    private static final String VERSION="{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[]},\"plant\":{\"ver\":1}}";
    private static final class Call {
        final String endpoint;final byte[] bytes;final ResponseListener callback;
        Call(String endpoint,byte[] bytes,ResponseListener callback){this.endpoint=endpoint;this.bytes=bytes.clone();this.callback=callback;}
        void success(String json){callback.onSuccess(json.getBytes(StandardCharsets.UTF_8));idle();}
        long id(){String text=new String(bytes,StandardCharsets.UTF_8);return Long.parseLong(text.split("&id=")[1].split("&")[0]);}
    }
    // Only SDK I/O is fake. Production connect/event/initSession/submit/poll/UI run.
    private static final class FakeDevice extends ESPDevice {
        final String version;final ArrayList<Call> calls=new ArrayList<>();ResponseListener session;int connections=0,disconnects=0;
        FakeDevice(Context context,String version){super(context,ESPConstants.TransportType.TRANSPORT_BLE,ESPConstants.SecurityType.SECURITY_1);this.version=version;}
        @Override public String getVersionInfo(){return version;}
        @Override public void connectBLEDevice(BluetoothDevice peripheral,String service){connections++;}
        @Override public void initSession(ResponseListener callback){session=callback;}
        @Override public void disconnectDevice(){disconnects++;}
        @Override public void sendDataToCustomEndPoint(String endpoint,byte[] bytes,ResponseListener callback){calls.add(new Call(endpoint,bytes,callback));}
        Call last(){return calls.get(calls.size()-1);}
    }
    private static FakeDevice connect(MainActivity activity,String version){
        invoke(activity,"cancelSession",new Class<?>[]{});set(activity,"kitName","PLANT-EBF528");set(activity,"draftSsid","Home");
        FakeDevice device=new FakeDevice(activity,version);
        int token=((SetupContract.Flow)get(activity,"flow")).token();
        invoke(activity,"connect",new Class<?>[]{ESPDevice.class,int.class},device,token);
        activity.onDeviceEvent(new DeviceConnectionEvent(ESPConstants.EVENT_DEVICE_CONNECTED));return device;
    }
    private static void authenticated(MainActivity activity,FakeDevice device,String state){
        assertNotNull(device.session);device.session.onSuccess(null);idle();
        assertEquals("plant-status",device.last().endpoint);device.last().success(state);
    }
    private static String status(long id,String state){return "{\"v\":1,\"id\":"+id+",\"rejected_id\":0,\"state\":\""+state+"\",\"url\":\""+(state.equals("connected")?"http://192.168.1.77/":"")+"\"}";}
    private static Call submit(MainActivity activity,FakeDevice device,String password){
        input(activity,"Mot de passe du Wi-Fi · vide si réseau ouvert").setText(password);
        find(root(activity),Button.class,"Connecter le kit").performClick();assertEquals("plant-config",device.last().endpoint);return device.last();
    }
    private static void enableSsidRead(MainActivity activity){
        Shadows.shadowOf((Application)activity.getApplication()).grantPermissions(Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION);
        Shadows.shadowOf((LocationManager)activity.getSystemService(Context.LOCATION_SERVICE)).setLocationEnabled(true);
    }
    private static WifiInfo wifi(String ssid){WifiInfo info=ShadowWifiInfo.newInstance();Shadows.shadowOf(info).setSSID(ssid);return info;}
    private static NetworkCapabilities capabilities(String ssid){
        NetworkCapabilities caps=ShadowNetworkCapabilities.newInstance();Shadows.shadowOf(caps).addTransportType(NetworkCapabilities.TRANSPORT_WIFI);Shadows.shadowOf(caps).setTransportInfo(wifi(ssid));return caps;
    }
    private static void permissionResult(MainActivity activity,String[] permissions,int[] results){
        // Android clears this platform flag before delivering the result. Direct
        // Robolectric callback injection must model that part of the SDK I/O too.
        org.robolectric.util.ReflectionHelpers.setField(activity,"mHasCurrentPermissionsRequest",false);
        activity.onRequestPermissionsResult(11,permissions,results);
    }
    @Test public void rejectedAdvertisedSecurityNeverStartsSessionOrReachesCredentials(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice device=connect(activity,VERSION.replace("\"sec_ver\":1","\"sec_ver\":0"));
            assertNull(device.session);assertTrue(device.calls.isEmpty());assertTrue(text(root(activity)).contains("canal sécurisé attendu"));
            assertNull(find(root(activity),Button.class,"Connecter le kit"));
        }
    }
    @Test @Config(sdk=31) public void bluetoothPermissionRequestAndRefusalKeepManualAndFallbackReachable(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").setText("Known home");
            find(root(activity),Button.class,"Associer mon kit avec son QR").performClick();
            org.robolectric.shadows.ShadowActivity.PermissionsRequest request=Shadows.shadowOf(activity).getLastRequestedPermission();
            assertEquals(11,request.requestCode);assertArrayEquals(new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},request.requestedPermissions);
            permissionResult(activity,request.requestedPermissions,new int[]{PackageManager.PERMISSION_DENIED,PackageManager.PERMISSION_DENIED});
            assertTrue(text(root(activity)).contains("Permission refusée"));assertTrue(text(root(activity)).contains("Le portail Wi-Fi reste un secours"));
            assertEquals("Known home",input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").getText().toString());
            find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();assertNotNull(input(activity,"Nom du kit"));assertNotNull(input(activity,"Code du kit · 16 caractères sans espace"));
            assertEquals(PackageManager.PERMISSION_DENIED,activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION));
        }
    }
    @Test @Config(sdk=31) public void grantedBluetoothPermissionsAdvanceToCameraWithoutSsidLocationPermission(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();Shadows.shadowOf(BluetoothAdapter.getDefaultAdapter()).setEnabled(true);
            input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").setText("Known home");find(root(activity),Button.class,"Associer mon kit avec son QR").performClick();
            String[] bluetooth={Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT};assertArrayEquals(bluetooth,Shadows.shadowOf(activity).getLastRequestedPermission().requestedPermissions);
            Shadows.shadowOf((Application)activity.getApplication()).grantPermissions(bluetooth);
            permissionResult(activity,bluetooth,new int[]{PackageManager.PERMISSION_GRANTED,PackageManager.PERMISSION_GRANTED});
            assertArrayEquals(new String[]{Manifest.permission.CAMERA},Shadows.shadowOf(activity).getLastRequestedPermission().requestedPermissions);
            assertEquals(PackageManager.PERMISSION_DENIED,activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION));assertNull(get(activity,"scanner"));
        }
    }
    @Test public void onlyMatchingCommittedPollCanPersistOrShowNewSuccess(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice device=connect(activity,VERSION);authenticated(activity,device,status(0,"idle"));
            Call submission=submit(activity,device,"WrongPassword");long id=submission.id();
            assertFalse(activity.getPreferences(0).contains("local_url"));submission.success("{\"queued\":true}");
            assertFalse(activity.getPreferences(0).contains("local_url"));device.last().success(status(id,"testing"));
            assertFalse(text(root(activity)).contains("Votre jardin est prêt"));nextPoll();device.last().success(status(id==1?2:1,"connected"));
            assertFalse(activity.getPreferences(0).contains("local_url"));assertFalse(text(root(activity)).contains("Votre jardin est prêt"));
            nextPoll();device.last().success(status(id,"connected"));
            assertTrue(text(root(activity)).contains("Votre jardin est prêt"));assertEquals("http://192.168.1.77/",activity.getPreferences(0).getString("local_url",""));
            assertEquals("http://plant-ebf528.local/",activity.getPreferences(0).getString("local_hostname_url",""));
            find(root(activity),Button.class,"Ouvrir par le nom du kit").performClick();Intent browser=Shadows.shadowOf(activity).getNextStartedActivity();
            assertEquals(Intent.ACTION_VIEW,browser.getAction());assertEquals("http://plant-ebf528.local/",browser.getDataString());
        }
    }
    @Test public void failedWifiCanBeCorrectedInTheSameAuthenticatedSession(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice device=connect(activity,VERSION);authenticated(activity,device,status(0,"idle"));
            Call wrong=submit(activity,device,"WrongPassword");wrong.success("{\"queued\":true}");device.last().success(status(wrong.id(),"failed"));
            assertTrue(text(root(activity)).contains("Connexion ou sauvegarde impossible"));
            Call corrected=submit(activity,device,"CorrectPassword");assertNotEquals(wrong.id(),corrected.id());corrected.success("{\"queued\":true}");
            device.last().success(status(wrong.id(),"connected"));assertFalse(activity.getPreferences(0).contains("local_url"));
            nextPoll();device.last().success(status(corrected.id(),"connected"));assertTrue(text(root(activity)).contains("Votre jardin est prêt"));
        }
    }
    @Test public void reassociationRecoversExistingCommitAndAllowsExplicitChange(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice device=connect(activity,VERSION);
            authenticated(activity,device,status(391,"connected"));
            assertTrue(text(root(activity)).contains("Configuration déjà enregistrée"));assertFalse(text(root(activity)).contains("Votre jardin est prêt"));
            assertEquals(1,device.calls.size());assertEquals("plant-status",device.last().endpoint);
            find(root(activity),Button.class,"Changer de Wi-Fi").performClick();assertNotNull(find(root(activity),Button.class,"Connecter le kit"));
            assertEquals(1,device.calls.size()); // No second SDK operation before/while the status read completes.
        }
    }
    @Test public void cancellingInitialReadDisconnectsWithoutConcurrentSdkRequest(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice device=connect(activity,VERSION);device.session.onSuccess(null);idle();Call reading=device.last();
            assertNull(find(root(activity),Button.class,"Changer de Wi-Fi"));
            find(root(activity),Button.class,"Annuler la vérification").performClick();assertEquals(1,device.calls.size());assertTrue(device.disconnects>0);
            reading.success(status(392,"connected"));assertFalse(activity.getPreferences(0).contains("local_url"));assertTrue(text(root(activity)).contains("Bienvenue au jardin"));
        }
    }
    @Test public void cancelledSdkCallbacksCannotAdvanceOrEraseNewSessionPayload(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();FakeDevice old=connect(activity,VERSION);authenticated(activity,old,status(0,"idle"));
            Call oldSubmit=submit(activity,old,"OldPassword");oldSubmit.success("{\"queued\":true}");Call oldPoll=old.last();
            find(root(activity),Button.class,"Quitter et vérifier plus tard").performClick();
            FakeDevice current=connect(activity,VERSION);authenticated(activity,current,status(0,"idle"));Call newSubmit=submit(activity,current,"NewPassword");
            byte[] payload=(byte[])get(activity,"pendingSecret");byte[] expected=payload.clone();assertTrue(payload.length>0);
            oldSubmit.success("{\"queued\":true}");oldSubmit.callback.onFailure(new Exception("old transport"));oldPoll.success(status(oldSubmit.id(),"connected"));old.session.onSuccess(null);idle();
            assertSame(payload,get(activity,"pendingSecret"));assertArrayEquals(expected,payload);assertEquals(2,current.calls.size());assertFalse(activity.getPreferences(0).contains("local_url"));
            newSubmit.success("{\"queued\":true}");assertNull(get(activity,"pendingSecret"));deviceStatus(current,newSubmit.id());assertTrue(text(root(activity)).contains("Votre jardin est prêt"));
        }
    }
    private static void deviceStatus(FakeDevice device,long id){device.last().success(status(id,"connected"));}
    @Test public void disabledBluetoothAtSearchPointIsAnActionableError(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();Shadows.shadowOf(BluetoothAdapter.getDefaultAdapter()).setEnabled(false);
            invoke(activity,"searchKit",new Class<?>[]{String.class,String.class},"PLANT-EBF528","0123456789ABCDEF");
            assertTrue(text(root(activity)).contains("Activez-le dans les réglages"));assertTrue(text(root(activity)).contains("réessayez l’association"));assertNull(get(activity,"scanner"));
        }
    }
    @Test public void backReturnsToRootThenFinishesAndAllInputsHaveLabels(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();EditText ssid=input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz");ssid.setText("PLANT-HOME");
            find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();
            assertNotEquals(View.NO_ID,input(activity,"Nom du kit").getId());assertFalse(input(activity,"Code du kit · 16 caractères sans espace").isSaveEnabled());
            activity.onBackPressed();assertFalse(activity.isFinishing());assertTrue(text(root(activity)).contains("Bienvenue au jardin"));activity.onBackPressed();assertTrue(activity.isFinishing());
        }
    }
    @Test public void legacyCurrentWifiPrefillsTheDisplayedField(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();enableSsidRead(activity);WifiManager wifi=(WifiManager)activity.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            Shadows.shadowOf(wifi).setConnectionInfo(wifi("Legacy kitchen"));
            find(root(activity),Button.class,"Utiliser le Wi-Fi de ce téléphone").performClick();idle();assertEquals("Legacy kitchen",input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").getText().toString());
        }
    }
    @Test @Config(sdk=31) public void registeredWifiCallbackPrefillsOnMainAndStaleReadsCannotOverwriteManualChoice(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();enableSsidRead(activity);ConnectivityManager connectivity=(ConnectivityManager)activity.getSystemService(Context.CONNECTIVITY_SERVICE);
            input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").setText("Initial");find(root(activity),Button.class,"Utiliser le Wi-Fi de ce téléphone").performClick();
            ConnectivityManager.NetworkCallback first=(ConnectivityManager.NetworkCallback)get(activity,"wifiCallback");assertTrue(Shadows.shadowOf(connectivity).getNetworkCallbacks().contains(first));
            first.onCapabilitiesChanged(ShadowNetwork.newInstance(1),capabilities("Current kitchen"));
            assertEquals("Initial",input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").getText().toString());idle();assertEquals("Current kitchen",input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").getText().toString());
            find(root(activity),Button.class,"Utiliser le Wi-Fi de ce téléphone").performClick();ConnectivityManager.NetworkCallback obsolete=(ConnectivityManager.NetworkCallback)get(activity,"wifiCallback");
            find(root(activity),Button.class,"Utiliser le Wi-Fi de ce téléphone").performClick();ConnectivityManager.NetworkCallback latest=(ConnectivityManager.NetworkCallback)get(activity,"wifiCallback");
            obsolete.onCapabilitiesChanged(ShadowNetwork.newInstance(2),capabilities("Obsolete"));idle();assertEquals("Current kitchen",input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").getText().toString());
            input(activity,"Nom du Wi-Fi de la maison · 2,4 GHz").setText("Manual home");find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();
            latest.onCapabilitiesChanged(ShadowNetwork.newInstance(3),capabilities("Too late"));idle();Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(4));
            assertEquals("Manual home",get(activity,"draftSsid"));assertEquals("PLANT-",input(activity,"Nom du kit").getText().toString());assertFalse(text(root(activity)).contains("Nom indisponible"));
        }
    }
    @Test public void refusedSsidPermissionStillAllowsManualWifiAndKitCode(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();View root=root(activity);
            assertNotEquals(0,activity.getWindow().getAttributes().flags&WindowManager.LayoutParams.FLAG_SECURE);
            find(root,EditText.class,null).setText("Home 2.4");
            find(root,Button.class,"Utiliser le Wi-Fi de ce téléphone").performClick();
            activity.onRequestPermissionsResult(11,new String[]{Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION},new int[]{PackageManager.PERMISSION_DENIED,PackageManager.PERMISSION_DENIED});
            assertTrue(text(root(activity)).contains("Permission refusée"));
            find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();
            assertTrue(text(root(activity)).contains("Associer à la main"));
            assertNotNull(find(root(activity),Button.class,"Trouver mon kit"));
        }
    }
    @Test public void cameraRefusalKeepsNonCameraPathAvailableAndBackgroundErasesCode(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();find(root(activity),EditText.class,null).setText("Home");
            activity.onRequestPermissionsResult(11,new String[]{Manifest.permission.CAMERA},new int[]{PackageManager.PERMISSION_DENIED});
            assertTrue(text(root(activity)).contains("caméra est facultative"));
            find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();
            ViewGroup page=(ViewGroup)find(root(activity),EditText.class,null).getParent();EditText proof=null;
            for(int i=0;i<page.getChildCount();i++)if(page.getChildAt(i) instanceof EditText)proof=(EditText)page.getChildAt(i);
            assertNotNull(proof);assertFalse(proof.isSaveEnabled());proof.setText("0123456789ABCDEF");
            controller.pause().stop();assertEquals("",proof.getText().toString());
            assertFalse(text(root(activity)).contains("0123456789ABCDEF"));
        }
    }
    @Test public void kitTemporaryWifiMustBeLeftBeforeAssociation(){
        try(ActivityController<MainActivity> controller=Robolectric.buildActivity(MainActivity.class).setup()){
            MainActivity activity=controller.get();find(root(activity),EditText.class,null).setText("PLANT-EBF528");
            find(root(activity),Button.class,"Saisir le nom et le code du kit").performClick();
            assertTrue(text(root(activity)).contains("Rejoignez d’abord le Wi-Fi de la maison"));
            assertNotNull(find(root(activity),Button.class,"Associer mon kit avec son QR"));
        }
    }
}
