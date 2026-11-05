package local.plantstation.kit;

import org.json.JSONObject;
import org.json.JSONArray;
import java.net.URI;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

/** Pure contract/policy, used by the actual UI and executable JVM tests. */
final class SetupContract {
    static final String SERVICE = "021a9004-0382-4aea-bff4-6b3f1c5adfb4";
    static boolean validKit(String name, String proof) {
        return name != null && name.matches("PLANT-[0-9A-F]{6}") && proof != null && proof.matches("[0-9A-F]{16}");
    }
    static final class KitIdentity {
        final String name,proof;
        KitIdentity(String name,String proof){this.name=name;this.proof=proof;}
    }
    static KitIdentity qr(String raw)throws Exception {
        if(raw==null||raw.getBytes(StandardCharsets.UTF_8).length>256)throw new Exception("qr");
        JSONObject json=new JSONObject(raw);
        if(!"v1".equals(json.getString("ver"))||!"ble".equals(json.getString("transport"))||!json.has("security")||json.getInt("security")!=1)throw new Exception("security");
        String name=json.getString("name"),proof=json.getString("pop");
        if(!validKit(name,proof))throw new Exception("kit");
        return new KitIdentity(name,proof);
    }
    static void requireSecurity1(String text) throws Exception {
        JSONObject root = new JSONObject(text);
        JSONObject prov = root.getJSONObject("prov");
        if (!prov.has("sec_ver") || prov.getInt("sec_ver") != 1 || !prov.getString("ver").equals("v1.1")) throw new Exception("security");
        JSONArray cap = prov.getJSONArray("cap");
        for (int i=0;i<cap.length();i++) if ("no_pop".equals(cap.getString(i))) throw new Exception("proof");
        if(root.getJSONObject("plant").getInt("ver") != 1) throw new Exception("version");
    }
    static String ssid(String raw) {
        if(raw==null || raw.equals("<unknown ssid>"))return "";
        if(raw.matches("0x[0-9A-Fa-f]+"))return ""; // Unquoted Android hex representation, not a quoted SSID.
        if(raw.length()>=2 && raw.startsWith("\"") && raw.endsWith("\""))raw=raw.substring(1,raw.length()-1);
        if(raw.isEmpty() || raw.indexOf('\uFFFD')>=0 || raw.getBytes(StandardCharsets.UTF_8).length>32)return "";
        return raw;
    }
    static boolean isKitWifi(String ssid) { return ssid.matches("PLANT-[0-9A-F]{6}"); }
    static String hostnameUrl(String kit) {
        return kit!=null&&kit.matches("PLANT-[0-9A-F]{6}")?"http://"+kit.toLowerCase(java.util.Locale.ROOT)+".local/":"";
    }
    static byte[] candidate(long id,String ssid,String password) throws Exception {
        int n=password.getBytes(StandardCharsets.UTF_8).length;
        if(id<1||id>0xffffffffL||ssid.isEmpty()||!StandardCharsets.UTF_8.newEncoder().canEncode(ssid)||!StandardCharsets.UTF_8.newEncoder().canEncode(password)||ssid.getBytes(StandardCharsets.UTF_8).length>32||ssid.chars().anyMatch(c -> c<32||c==127)||
           password.chars().anyMatch(c -> c<32||c==127)||!(n==0||(n>=8&&n<=63)||(n==64&&password.matches("[a-fA-F0-9]{64}"))))throw new Exception("credentials");
        byte[] bytes=("v=1&id="+id+"&ssid="+URLEncoder.encode(ssid,"UTF-8")+"&password="+URLEncoder.encode(password,"UTF-8")).getBytes(StandardCharsets.UTF_8);
        if(bytes.length>512)throw new Exception("limit");
        return bytes;
    }
    static boolean localUrl(String url,String kit) {
        try {
            URI uri=new URI(url);
            if(!"http".equals(uri.getScheme())||uri.getUserInfo()!=null||uri.getPort()!=-1||uri.getFragment()!=null||uri.getQuery()!=null||!("/".equals(uri.getPath())||"".equals(uri.getPath())))return false;
            String host=uri.getHost();
            if(host==null)return false;
            if(host.equalsIgnoreCase(kit.toLowerCase(java.util.Locale.ROOT)+".local"))return true;
            if(!host.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}"))return false;
            String[] parts=host.split("\\.");int[] n=new int[4];
            for(int i=0;i<4;i++){n[i]=Integer.parseInt(parts[i]);if(n[i]>255||(parts[i].length()>1&&parts[i].startsWith("0")))return false;}
            return n[0]==10||(n[0]==192&&n[1]==168)||(n[0]==172&&n[1]>=16&&n[1]<=31)||(n[0]==169&&n[1]==254);
        }catch(Exception e){return false;}
    }
    static boolean needsLocationForBle(int api) { return api<31; }
    static final class Flow {
        private volatile int generation=0;
        boolean secure=false,busy=false;
        long attempt=0;
        int reset(){secure=busy=false;attempt=0;return ++generation;}
        int token(){return generation;}
        boolean current(int token){return generation==token;}
        boolean begin(long id){if(!secure||busy)return false;busy=true;attempt=id;return true;}
        boolean matches(long id){return busy&&attempt==id;}
    }
}
