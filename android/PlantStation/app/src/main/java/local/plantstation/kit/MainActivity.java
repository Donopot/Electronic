package local.plantstation.kit;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.location.LocationManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.Uri;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.Preview;
import androidx.camera.core.ImageAnalysis;
import com.google.mlkit.vision.barcode.BarcodeScanner;
import com.google.mlkit.vision.barcode.BarcodeScanning;
import com.google.mlkit.vision.common.InputImage;
import com.espressif.provisioning.DeviceConnectionEvent;
import com.espressif.provisioning.ESPConstants;
import com.espressif.provisioning.ESPDevice;
import com.espressif.provisioning.listeners.ResponseListener;
import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** One foreground-only setup session. Nothing containing credentials is saved. */
public final class MainActivity extends AppCompatActivity {
    private final Handler ui=new Handler(Looper.getMainLooper());
    private final SetupContract.Flow flow=new SetupContract.Flow();
    private LinearLayout page;
    private TextView notice;
    private EditText ssidField,passwordField,nameField,proofField;
    private ESPDevice device;
    private String draftSsid="",kitName="",localUrl="",hostnameUrl="";
    private ConnectivityManager.NetworkCallback wifiCallback;
    private BluetoothLeScanner scanner;
    private ScanCallback scanCallback;
    private Runnable timeout;
    private boolean connecting=false,readingSsid=false;
    private int permissionAction=0;
    private byte[] pendingSecret;
    private ProcessCameraProvider cameraProvider;
    private ExecutorService cameraExecutor;
    private BarcodeScanner barcodeScanner;
    private boolean qrAccepted=false;
    private boolean atRoot=false,recovering=false;
    private int ssidReadGeneration=0,recoveryGeneration=0;

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        EventBus.getDefault().register(this);
        localUrl=getPreferences(MODE_PRIVATE).getString("local_url","");
        hostnameUrl=getPreferences(MODE_PRIVATE).getString("local_hostname_url",SetupContract.hostnameUrl(getPreferences(MODE_PRIVATE).getString("kit_name","")));
        home("");
    }
    private int px(int dp){return Math.round(dp*getResources().getDisplayMetrics().density);}
    private void screen(String title,String intro){
        stopWifiRead();atRoot=false;
        ScrollView scroll=new ScrollView(this);page=new LinearLayout(this);page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(px(24),px(40),px(24),px(24));page.setBackgroundColor(Color.rgb(244,240,229));scroll.addView(page);setContentView(scroll);
        androidx.core.view.ViewCompat.setOnApplyWindowInsetsListener(scroll,(view,insets)->{
            androidx.core.graphics.Insets bars=insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.systemBars());
            page.setPadding(px(24)+bars.left,px(24)+bars.top,px(24)+bars.right,px(24)+bars.bottom);return insets;
        });
        androidx.core.view.ViewCompat.requestApplyInsets(scroll);
        text("MES PLANTES",13);text(title,29);text(intro,17);notice=text("",16);
    }
    private TextView text(String label,int size){TextView v=new TextView(this);v.setText(label);v.setTextSize(size);v.setTextColor(Color.rgb(39,65,47));v.setPadding(0,px(8),0,px(8));page.addView(v);return v;}
    private EditText field(String label,boolean secret){
        TextView caption=text(label,15);EditText v=new EditText(this);v.setId(View.generateViewId());caption.setLabelFor(v.getId());v.setSingleLine(true);v.setTextSize(18);v.setSaveEnabled(false);
        v.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        v.setInputType(InputType.TYPE_CLASS_TEXT|(secret?InputType.TYPE_TEXT_VARIATION_PASSWORD:InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS));page.addView(v);return v;
    }
    private Button button(String label,Runnable action){Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setPadding(px(12),px(10),px(12),px(10));page.addView(b);b.setOnClickListener(v->action.run());return b;}
    private void home(String message){
        screen("Bienvenue au jardin", "Gardez votre téléphone sur le Wi-Fi de la maison. Le kit recevra ses réglages par Bluetooth.");
        atRoot=true;
        ssidField=field("Nom du Wi-Fi de la maison · 2,4 GHz",false);ssidField.setText(draftSsid);
        button("Utiliser le Wi-Fi de ce téléphone",()->requestSsid());
        button("Associer mon kit avec son QR",()->{draftSsid=ssidField.getText().toString();if(checkSsid())permissionsForBle(2);});
        button("Saisir le nom et le code du kit",()->{draftSsid=ssidField.getText().toString();if(checkSsid())manual();});
        text("Le nom reste modifiable. Android ne communique pas le mot de passe enregistré : vous le saisirez une fois.",15);
        if(!localUrl.isEmpty())button("Ouvrir mes plantes",()->openLocal(localUrl));
        offerHostname();
        text("Secours sans app : PRG court fait défiler le QR Wi-Fi du kit puis http://192.168.4.1/.",15);
        notice.setText(message);
    }
    private boolean checkSsid(){
        if(draftSsid.isEmpty()||draftSsid.getBytes(StandardCharsets.UTF_8).length>32){notice.setText("Saisissez le nom exact du Wi-Fi (32 octets maximum).");return false;}
        if(SetupContract.isKitWifi(draftSsid)){notice.setText("Rejoignez d’abord le Wi-Fi de la maison dans les réglages du téléphone, puis revenez ici.");return false;}return true;
    }
    private boolean granted(String permission){return checkSelfPermission(permission)==PackageManager.PERMISSION_GRANTED;}
    private boolean locationEnabled(){LocationManager manager=(LocationManager)getSystemService(LOCATION_SERVICE);return Build.VERSION.SDK_INT>=28?manager.isLocationEnabled():manager.isProviderEnabled(LocationManager.GPS_PROVIDER)||manager.isProviderEnabled(LocationManager.NETWORK_PROVIDER);}
    private void requestSsid(){
        draftSsid=ssidField.getText().toString();
        if(!granted(Manifest.permission.ACCESS_FINE_LOCATION)){
            notice.setText("Android demande la localisation pour lire le nom du Wi-Fi. Vous pouvez aussi le saisir sans cette permission.");
            permissionAction=1;requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION},11);return;
        }
        if(!locationEnabled()){notice.setText("La localisation système est désactivée. Activez-la pour lire le nom, ou saisissez-le directement.");return;}
        stopWifiRead();readingSsid=true;final int token=ssidReadGeneration;
        try {
            ConnectivityManager cm=(ConnectivityManager)getSystemService(CONNECTIVITY_SERVICE);
            if(Build.VERSION.SDK_INT>=31){
                wifiCallback=new ConnectivityManager.NetworkCallback(ConnectivityManager.NetworkCallback.FLAG_INCLUDE_LOCATION_INFO){
                    @Override public void onCapabilitiesChanged(Network network,NetworkCapabilities caps){
                        if(caps.getTransportInfo() instanceof WifiInfo){String value=((WifiInfo)caps.getTransportInfo()).getSSID();ui.post(()->receivedSsid(token,value));}
                    }
                };
                cm.registerNetworkCallback(new NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(),wifiCallback);
                ui.postDelayed(()->{if(token==ssidReadGeneration&&readingSsid){stopWifiRead();notice.setText("Nom indisponible : saisissez-le directement.");}},4000);
            }else{
                WifiManager wm=(WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);receivedSsid(token,wm.getConnectionInfo().getSSID());
            }
        }catch(SecurityException e){stopWifiRead();notice.setText("Nom masqué par Android : saisissez-le directement.");}
    }
    private void receivedSsid(int token,String raw){
        if(token!=ssidReadGeneration||!readingSsid)return;stopWifiRead();String value=SetupContract.ssid(raw);
        if(value.isEmpty()){notice.setText("Nom masqué par Android : saisissez-le directement.");return;}
        draftSsid=value;ssidField.setText(value);notice.setText(SetupContract.isKitWifi(value)?"Vous êtes sur le Wi-Fi du kit. Rejoignez celui de la maison avant de continuer.":"Nom du Wi-Fi prérempli. Vous pouvez le corriger.");
    }
    private void stopWifiRead(){++ssidReadGeneration;readingSsid=false;if(wifiCallback!=null){try{((ConnectivityManager)getSystemService(CONNECTIVITY_SERVICE)).unregisterNetworkCallback(wifiCallback);}catch(Exception ignored){}wifiCallback=null;}}
    private void permissionsForBle(int action){
        if(Build.VERSION.SDK_INT>=31&&(!granted(Manifest.permission.BLUETOOTH_SCAN)||!granted(Manifest.permission.BLUETOOTH_CONNECT))){
            permissionAction=action;requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},11);return;
        }
        if(SetupContract.needsLocationForBle(Build.VERSION.SDK_INT)&&!granted(Manifest.permission.ACCESS_FINE_LOCATION)){
            permissionAction=action;requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION,Manifest.permission.ACCESS_FINE_LOCATION},11);return;
        }
        if(SetupContract.needsLocationForBle(Build.VERSION.SDK_INT)&&!locationEnabled()){notice.setText("Cet Android demande la localisation activée pour trouver le kit Bluetooth. Le portail Wi-Fi reste accessible en secours.");return;}
        try{BluetoothAdapter adapter=((BluetoothManager)getSystemService(BLUETOOTH_SERVICE)).getAdapter();
            if(adapter==null||!adapter.isEnabled()){notice.setText("Activez Bluetooth dans les réglages du téléphone puis réessayez.");return;}
        }catch(SecurityException e){notice.setText("Permission Bluetooth retirée. Autorisez-la puis réessayez.");return;}
        if(action==2){if(!granted(Manifest.permission.CAMERA)){permissionAction=2;requestPermissions(new String[]{Manifest.permission.CAMERA},11);return;}scanQr();}
        else findManual();
    }
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){
        super.onRequestPermissionsResult(code,permissions,results);if(code!=11)return;int action=permissionAction;permissionAction=0;
        for(int result:results)if(result!=PackageManager.PERMISSION_GRANTED){notice.setText("Permission refusée. La saisie du Wi-Fi reste accessible ; la caméra est facultative avec le nom et le code du kit. Le portail Wi-Fi reste un secours.");return;}
        if(action==1)requestSsid();else permissionsForBle(action);
    }
    @androidx.annotation.OptIn(markerClass=androidx.camera.core.ExperimentalGetImage.class) private void scanQr(){
        cancelSession();final int token=flow.token();
        screen("Associer le kit","Scannez le QR Android sur l’écran du kit. PRG 8 secondes le rouvre.");
        PreviewView preview=new PreviewView(this);page.addView(preview,new LinearLayout.LayoutParams(-1,px(280)));
        button("Saisir le code à la place",()->{cancelSession();manual();});
        deadline(token,45000,"QR ou kit introuvable. Rapprochez le téléphone, vérifiez l’écran Android du kit, puis réessayez.");
        qrAccepted=false;cameraExecutor=Executors.newSingleThreadExecutor();barcodeScanner=BarcodeScanning.getClient();final BarcodeScanner sessionScanner=barcodeScanner;
        com.google.common.util.concurrent.ListenableFuture<ProcessCameraProvider> future=ProcessCameraProvider.getInstance(this);
        future.addListener(()->{
            if(!flow.current(token))return;
            try{
                cameraProvider=future.get();Preview cameraPreview=new Preview.Builder().build();cameraPreview.setSurfaceProvider(preview.getSurfaceProvider());
                ImageAnalysis analysis=new ImageAnalysis.Builder().setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST).build();
                analysis.setAnalyzer(cameraExecutor,image->{
                    if(!flow.current(token)||image.getImage()==null){image.close();return;}
                    try{sessionScanner.process(InputImage.fromMediaImage(image.getImage(),image.getImageInfo().getRotationDegrees()))
                        .addOnSuccessListener(barcodes->{
                            if(!flow.current(token)||qrAccepted)return;
                            for(com.google.mlkit.vision.barcode.common.Barcode barcode:barcodes){
                                String raw=barcode.getRawValue();if(raw==null)continue;qrAccepted=true;
                                try{SetupContract.KitIdentity identity=SetupContract.qr(raw);stopCamera();searchKit(identity.name,identity.proof);}
                                catch(Exception e){fail("Ce QR ne correspond pas au QR Android sécurisé du kit. Affichez-le avec PRG, ou saisissez son code.");}break;
                            }
                        }).addOnFailureListener(error->{if(flow.current(token))notice.setText("Lecture du QR impossible. Réessayez ou saisissez le code.");})
                        .addOnCompleteListener(done->image.close());}catch(Exception e){image.close();}
                });
                cameraProvider.bindToLifecycle(this,CameraSelector.DEFAULT_BACK_CAMERA,cameraPreview,analysis);
            }catch(Exception e){fail("Caméra indisponible. Saisissez le nom et le code du kit.");}
        },androidx.core.content.ContextCompat.getMainExecutor(this));
    }
    private void manual(){
        screen("Associer à la main","Recopiez le nom et les deux lignes du code affichés sur le kit. Gardez ce code privé.");
        nameField=field("Nom du kit",false);nameField.setText("PLANT-");proofField=field("Code du kit · 16 caractères sans espace",true);
        button("Trouver mon kit",()->permissionsForBle(3));button("Retour",()->{cancelSession();home("");});
    }
    private void findManual(){
        String name=nameField.getText().toString(),proof=proofField.getText().toString();
        if(!SetupContract.validKit(name,proof)){notice.setText("Vérifiez le nom PLANT-… et le code de 16 caractères, sans espace.");return;}
        searchKit(name,proof);
    }
    private void searchKit(String name,String proof){
        cancelSession();kitName=name;final int token=flow.token();
        ESPDevice found=new ESPDevice(getApplicationContext(),ESPConstants.TransportType.TRANSPORT_BLE,ESPConstants.SecurityType.SECURITY_1);
        found.setDeviceName(name);found.setProofOfPossession(proof);device=found;
        screen("Recherche du kit","Restez près du kit. Son écran doit proposer la configuration Android.");button("Annuler",()->{cancelSession();home("");});
        try{
            BluetoothAdapter adapter=((BluetoothManager)getSystemService(BLUETOOTH_SERVICE)).getAdapter();
            scanner=adapter!=null&&adapter.isEnabled()?adapter.getBluetoothLeScanner():null;
            if(scanner==null){fail("Bluetooth est désactivé ou indisponible. Activez-le dans les réglages puis réessayez l’association.");return;}
        }catch(SecurityException e){fail("Permission Bluetooth retirée. Autorisez-la puis réessayez l’association.");return;}
        scanCallback=new ScanCallback(){
            @Override public void onScanResult(int type,ScanResult result){ui.post(()->{
                if(!flow.current(token)||connecting||device!=found||scanner==null)return;
                String advertised=result.getScanRecord()==null?null:result.getScanRecord().getDeviceName();
                if(!name.equals(advertised))return;stopScan();found.setBluetoothDevice(result.getDevice());found.setPrimaryServiceUuid(SetupContract.SERVICE);connect(found,token);
            });}
            @Override public void onScanFailed(int error){ui.post(()->{if(flow.current(token))fail("Recherche Bluetooth indisponible. Réessayez après avoir activé Bluetooth.");});}
        };
        try{scanner.startScan(scanCallback);deadline(token,20000,"Kit introuvable. Rapprochez-le et rouvrez la configuration avec PRG 8 secondes.");}catch(SecurityException e){fail("Permission Bluetooth indisponible. Autorisez-la puis réessayez.");}catch(IllegalStateException e){fail("Bluetooth est désactivé. Activez-le puis réessayez l’association.");}
    }
    private void connect(ESPDevice found,int token){
        stopCamera();stopScan();device=found;connecting=true;
        screen("Connexion au kit","Vérification du code et du canal sécurisé…");button("Annuler",()->{cancelSession();home("");});
        deadline(token,20000,"Connexion interrompue. Rapprochez le kit puis recommencez l’association.");
        try{found.connectBLEDevice(found.getBluetoothDevice(),SetupContract.SERVICE);}catch(SecurityException e){fail("Permission Bluetooth indisponible. Réessayez.");}catch(Exception e){fail("Connexion Bluetooth impossible. Réessayez.");}
    }
    @Subscribe(threadMode=ThreadMode.MAIN) public void onDeviceEvent(DeviceConnectionEvent event){
        if(device==null)return;final int token=flow.token();
        if(event.getEventType()==ESPConstants.EVENT_DEVICE_CONNECTED&&connecting){
            if(device.getVersionInfo()==null)return; // A stale event from a retired SDK transport.
            // The SDK can otherwise downgrade automatically inside initSession().
            try{SetupContract.requireSecurity1(device.getVersionInfo());}catch(Exception e){fail("Ce kit ne propose pas le canal sécurisé attendu. Aucun réglage Wi-Fi n’a été envoyé.");return;}
            connecting=false;deadline(token,15000,"Le code du kit n’a pas été confirmé. Vérifiez-le puis recommencez.");
            device.initSession(new ResponseListener(){
                public void onSuccess(byte[] data){ui.post(()->{if(flow.current(token)&&device!=null&&device.getSecurityType()==ESPConstants.SecurityType.SECURITY_1){flow.secure=true;clearDeadline();checkExistingSetup(token);}});}
                public void onFailure(Exception e){ui.post(()->{if(flow.current(token))fail("Code incorrect ou connexion interrompue. Réassociez le kit puis réessayez.");});}
            });
        }else if(event.getEventType()==ESPConstants.EVENT_DEVICE_DISCONNECTED&&flow.secure){
            BluetoothManager manager=(BluetoothManager)getSystemService(BLUETOOTH_SERVICE);
            try{if(device.getBluetoothDevice()!=null&&manager.getConnectionState(device.getBluetoothDevice(),android.bluetooth.BluetoothProfile.GATT)==android.bluetooth.BluetoothProfile.STATE_CONNECTED)return;}catch(SecurityException e){fail("Permission Bluetooth retirée. Autorisez-la pour reprendre l’association.");return;}
            fail("Connexion Bluetooth interrompue. Le réseau enregistré reste conservé si le nouvel essai n’a pas été confirmé. Réassociez le kit pour vérifier.");
        }
    }
    private void password(){
        recovering=false;++recoveryGeneration;clearDeadline();
        screen("Connecter la maison","Kit associé. Saisissez le mot de passe du Wi-Fi domestique ; il reste uniquement en mémoire pendant cet essai.");
        ssidField=field("Nom du Wi-Fi · 2,4 GHz",false);ssidField.setText(draftSsid);passwordField=field("Mot de passe du Wi-Fi · vide si réseau ouvert",true);
        button("Connecter le kit",()->submit());button("Recommencer l’association",()->{cancelSession();home("");});
    }
    private void submit(){
        if(flow.busy||!flow.secure||device==null)return;draftSsid=ssidField.getText().toString();if(!checkSsid())return;
        long id=(new SecureRandom().nextInt()&0xffffffffL);if(id==0)id=1;
        try{pendingSecret=SetupContract.candidate(id,draftSsid,passwordField.getText().toString());}catch(Exception e){notice.setText("Vérifiez le nom et le mot de passe : vide, 8 à 63 octets, ou une clé hexadécimale de 64 caractères.");return;}
        passwordField.setText("");if(!flow.begin(id)){erasePayload();return;}final int token=flow.token();final long attempt=id;
        screen("Le kit rejoint votre Wi-Fi","Connexion et sauvegarde en cours. Le téléphone reste sur son Wi-Fi. Patientez jusqu’à la confirmation.");
        button("Quitter et vérifier plus tard",()->{cancelSession();home("L’essai du kit peut se terminer pendant votre absence. Réassociez-le pour continuer ; aucun succès n’est supposé.");});
        deadline(token,40000,"Aucune confirmation reçue. Réassociez le kit pour vérifier ; l’ancien réseau est conservé tant que le nouveau n’est pas connecté et sauvegardé.");
        device.sendDataToCustomEndPoint("plant-config",pendingSecret,new ResponseListener(){
            public void onSuccess(byte[] data){ui.post(()->{if(!flow.current(token)||!flow.matches(attempt))return;erasePayload();try{JSONObject reply=json(data);if(!reply.optBoolean("queued",false))throw new Exception();poll(token);}catch(Exception e){fail("Le kit est occupé ou les réglages ont été refusés. Réassociez-le puis réessayez.");}});}
            public void onFailure(Exception e){ui.post(()->{if(!flow.current(token)||!flow.matches(attempt))return;erasePayload();fail("Envoi interrompu. Réassociez le kit pour vérifier et réessayer.");});}
        });
    }
    private JSONObject json(byte[] data)throws Exception {if(data==null||data.length>384)throw new Exception();return new JSONObject(new String(data,StandardCharsets.UTF_8));}
    private void poll(int token){
        if(!flow.current(token)||device==null||!flow.busy)return;
        final long attempt=flow.attempt;
        device.sendDataToCustomEndPoint("plant-status",new byte[]{'1'},new ResponseListener(){
            public void onSuccess(byte[] data){ui.post(()->{
                if(!flow.current(token)||!flow.matches(attempt))return;
                try{
                    JSONObject status=json(data);if(status.getInt("v")!=1)throw new Exception();
                    if(status.optLong("rejected_id",0)==flow.attempt){fail("Le kit traite déjà une autre demande. Réessayez après quelques secondes.");return;}
                    if(flow.matches(status.getLong("id"))){
                        String state=status.getString("state");
                        if(state.equals("failed")){clearDeadline();flow.busy=false;password();notice.setText("Connexion ou sauvegarde impossible. Vérifiez le nom et le mot de passe, puis réessayez. L’ancien réseau reste enregistré.");return;}
                        if(state.equals("connected")){
                            completed(status.getString("url"),false);return;
                        }
                    }
                    ui.postDelayed(()->poll(token),750);
                }catch(Exception e){fail("Réponse du kit invalide. Réassociez-le pour vérifier la configuration.");}
            });}
            public void onFailure(Exception e){ui.post(()->{if(flow.current(token)&&flow.matches(attempt))fail("Confirmation interrompue. Réassociez le kit pour vérifier ; aucun succès n’est supposé.");});}
        });
    }
    private void checkExistingSetup(int token){
        recovering=true;final int read=++recoveryGeneration;
        screen("Vérifier le kit","Lecture de la configuration déjà enregistrée dans le kit…");
        button("Annuler la vérification",()->{cancelSession();home("Vérification annulée. Réassociez le kit pour reprendre.");});
        deadline(token,40000,"Vérification interrompue. Réassociez le kit pour vérifier son état.");
        readExistingSetup(token,read);
    }
    private void readExistingSetup(int token,int read){
        if(!flow.current(token)||!recovering||read!=recoveryGeneration||device==null)return;
        device.sendDataToCustomEndPoint("plant-status",new byte[]{'1'},new ResponseListener(){
            public void onSuccess(byte[] data){ui.post(()->{
                if(!flow.current(token)||!recovering||read!=recoveryGeneration)return;
                try{
                    JSONObject status=json(data);if(status.getInt("v")!=1)throw new Exception();
                    String state=status.getString("state");
                    if(state.equals("connected")){completed(status.getString("url"),true);return;}
                    if(state.equals("testing")){notice.setText("Un essai du kit est encore en cours. Attendez sa confirmation, ou choisissez de changer de Wi-Fi.");ui.postDelayed(()->readExistingSetup(token,read),750);return;}
                    if(!state.equals("idle")&&!state.equals("failed"))throw new Exception();
                    password();
                }catch(Exception e){fail("État du kit invalide. Réassociez-le pour vérifier sa configuration.");}
            });}
            public void onFailure(Exception e){ui.post(()->{if(flow.current(token)&&recovering&&read==recoveryGeneration)fail("Vérification interrompue. Réassociez le kit pour vérifier son état.");});}
        });
    }
    private void completed(String url,boolean recovered)throws Exception{
        if(!SetupContract.localUrl(url,kitName))throw new Exception();
        localUrl=url;hostnameUrl=SetupContract.hostnameUrl(kitName);
        getPreferences(MODE_PRIVATE).edit().putString("local_url",url).putString("kit_name",kitName).putString("local_hostname_url",hostnameUrl).apply();
        if(recovered){recovering=false;++recoveryGeneration;clearDeadline();flow.busy=false;flow.attempt=0;device.setProofOfPossession("");}
        else cancelSession();
        screen(recovered?"Configuration déjà enregistrée":"Votre jardin est prêt",recovered?"Le kit confirme qu’un précédent réglage est connecté et sauvegardé.":"Le Wi-Fi est connecté et sauvegardé dans le kit. Votre téléphone peut ouvrir le tableau de bord local.");
        button("Ouvrir mes plantes",()->openLocal(localUrl));text(localUrl,16);offerHostname();
        button("Changer de Wi-Fi",()->{if(device!=null&&flow.secure)password();else home("Maintenez PRG 8 secondes pour rouvrir l’association et changer de Wi-Fi.");});
    }
    private void offerHostname(){
        if(!hostnameUrl.isEmpty()){
            text("Si l’adresse Wi-Fi du kit a changé, utilisez son nom permanent.",15);
            button("Ouvrir par le nom du kit",()->openLocal(hostnameUrl));
        }
    }
    private void openLocal(String url){
        String name=getPreferences(MODE_PRIVATE).getString("kit_name",kitName);
        if(!SetupContract.localUrl(url,name)){notice.setText("Adresse locale invalide. Reconfigurez le kit.");return;}
        try{startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(url)));}catch(Exception e){notice.setText("Aucun navigateur disponible. Ouvrez l’adresse locale affichée depuis le Wi-Fi de la maison.");}
    }
    private void deadline(int token,int milliseconds,String message){clearDeadline();timeout=()->{if(flow.current(token))fail(message);};ui.postDelayed(timeout,milliseconds);}
    private void clearDeadline(){if(timeout!=null)ui.removeCallbacks(timeout);timeout=null;}
    private void erasePayload(){if(pendingSecret!=null)Arrays.fill(pendingSecret,(byte)0);pendingSecret=null;}
    private void stopScan(){if(scanner!=null&&scanCallback!=null)try{scanner.stopScan(scanCallback);}catch(SecurityException ignored){}scanner=null;scanCallback=null;}
    private void stopCamera(){
        if(cameraProvider!=null){cameraProvider.unbindAll();cameraProvider=null;}
        if(cameraExecutor!=null){cameraExecutor.shutdown();cameraExecutor=null;}
        if(barcodeScanner!=null){barcodeScanner.close();barcodeScanner=null;}
    }
    private void cancelSession(){
        recovering=false;++recoveryGeneration;
        flow.reset();clearDeadline();stopWifiRead();stopScan();stopCamera();erasePayload();connecting=false;
        if(passwordField!=null)passwordField.setText("");if(proofField!=null)proofField.setText("");
        ESPDevice old=device;device=null;if(old!=null){old.setProofOfPossession("");try{old.disconnectDevice();}catch(Exception ignored){}}
    }
    private void fail(String message){cancelSession();home(message);}
    @Override public void onBackPressed(){boolean root=atRoot;cancelSession();if(root)finish();else home("");}
    @Override protected void onStop(){cancelSession();super.onStop();if(!isFinishing())home("Reprenez l’association pour continuer. Aucun mot de passe n’a été conservé.");}
    @Override protected void onDestroy(){cancelSession();EventBus.getDefault().unregister(this);super.onDestroy();}
}
