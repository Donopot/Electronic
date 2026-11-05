package local.plantstation.kit;
import org.junit.Test;
import static org.junit.Assert.*;
import java.nio.charset.StandardCharsets;

public final class SetupContractTest {
    private static void refused(String version){try{SetupContract.requireSecurity1(version);fail("accepted unsafe version");}catch(Exception expected){}}
    @Test public void refusesDowngradeMissingVersionAndNoProof()throws Exception{
        SetupContract.requireSecurity1("{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[]},\"plant\":{\"ver\":1}}");
        refused("{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":0,\"cap\":[]},\"plant\":{\"ver\":1}}");
        refused("{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":2,\"cap\":[]},\"plant\":{\"ver\":1}}");
        refused("{\"prov\":{\"ver\":\"v1.1\",\"cap\":[]},\"plant\":{\"ver\":1}}");
        refused("{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[\"no_pop\"]},\"plant\":{\"ver\":1}}");
        refused("invalid");
    }
    @Test public void associatesOnlyTheNamedKitAndFullCode(){
        assertTrue(SetupContract.validKit("PLANT-EBF528","0123456789ABCDEF"));
        assertFalse(SetupContract.validKit("Other","0123456789ABCDEF"));
        assertFalse(SetupContract.validKit("PLANT-EBF528","01234567 89ABCDEF"));
        assertFalse(SetupContract.validKit("PLANT-EBF528",""));
    }
    @Test public void qrRequiresExplicitSecurity1AndRecognizedFormat()throws Exception{
        String qr="{\"ver\":\"v1\",\"name\":\"PLANT-EBF528\",\"transport\":\"ble\",\"security\":1,\"pop\":\"0123456789ABCDEF\"}";
        assertEquals("PLANT-EBF528",SetupContract.qr(qr).name);
        for(String bad:new String[]{qr.replace("\"security\":1,",""),qr.replace("\"security\":1","\"security\":0"),qr.replace("v1","v2"),qr.replace("ble","softap"),"x".repeat(257)}){
            try{SetupContract.qr(bad);fail("unsafe qr accepted");}catch(Exception expected){}
        }
    }
    @Test public void currentSsidKeepsInteriorQuotesAndSupportsManualFallback(){
        assertEquals("Ma \"box\"",SetupContract.ssid("\"Ma \"box\"\""));
        assertEquals("Bébé 🌿",SetupContract.ssid("\"Bébé 🌿\""));
        assertEquals("",SetupContract.ssid("<unknown ssid>"));assertEquals("",SetupContract.ssid(null));
        assertEquals("",SetupContract.ssid("0xFFFF"));assertEquals("",SetupContract.ssid("\uFFFD"));
        assertEquals("0xKitchen",SetupContract.ssid("\"0xKitchen\""));
        assertEquals("0xFFFF",SetupContract.ssid("\"0xFFFF\""));
        assertFalse(SetupContract.isKitWifi("PLANT-HOME"));
        assertFalse(SetupContract.isKitWifi("PLANT-EBF528-Guest"));
        assertTrue(SetupContract.isKitWifi("PLANT-EBF528"));
        assertFalse(SetupContract.needsLocationForBle(31));assertTrue(SetupContract.needsLocationForBle(30));
        // No SSID-read permission is a prerequisite for a known, manually typed SSID.
        try{assertTrue(SetupContract.candidate(1,"Known home","password").length>0);}catch(Exception e){fail();}
    }
    @Test public void candidateIsVersionedCorrelatedEncodedAndBounded()throws Exception{
        String payload=new String(SetupContract.candidate(4294967295L,"Ma box & 🌿","mot+de%passe"),StandardCharsets.UTF_8);
        assertTrue(payload.startsWith("v=1&id=4294967295&ssid="));assertTrue(payload.contains("%26"));assertTrue(payload.contains("password=mot%2Bde%25passe"));
        assertTrue(SetupContract.candidate(1,"S".repeat(32),"A".repeat(64)).length<=512);
        assertTrue(SetupContract.candidate(1,"Open","").length>0);
        for(String[] values:new String[][]{{"S".repeat(33),"password"},{"Home","short"},{"Home","Z".repeat(64)},{"Home\n","password"},{"Home","pass\u0000word"}}){
            try{SetupContract.candidate(1,values[0],values[1]);fail("invalid accepted");}catch(Exception expected){}
        }
    }
    @Test public void localLinksRejectRemoteUserinfoFragmentsPortsAndOtherKits(){
        for(String url:new String[]{"http://192.168.2.99/","http://10.0.0.2/","http://172.16.0.3/","http://plant-ebf528.local/"})assertTrue(url,SetupContract.localUrl(url,"PLANT-EBF528"));
        for(String url:new String[]{"https://192.168.1.2/","http://8.8.8.8/","http://evil.example/","http://plant-aaaaaa.local/","http://x@192.168.1.1/","http://192.168.1.1:80/","http://192.168.1.1/#x","http://192.168.1.1/?p=1","http://192.168.1.1/path","http://192.168.999.1/","http://127.0.0.1/","http://192.168.01.1/"})assertFalse(url,SetupContract.localUrl(url,"PLANT-EBF528"));
    }
    @Test public void staleCallbacksAndDuplicateSubmissionsCannotAdvanceSession(){
        SetupContract.Flow flow=new SetupContract.Flow();int old=flow.token();
        assertFalse(flow.begin(3));flow.secure=true;assertTrue(flow.begin(3));assertFalse(flow.begin(4));
        assertFalse(flow.matches(2));assertTrue(flow.matches(3));flow.reset();assertFalse(flow.current(old));
        assertFalse(flow.secure);assertFalse(flow.busy);assertFalse(flow.begin(4));
        flow.secure=true;assertTrue(flow.begin(4));assertTrue(flow.matches(4));assertFalse(flow.matches(3));
        // A failed Wi-Fi attempt permits a corrected attempt in the same secure session.
        flow.busy=false;assertTrue(flow.begin(5));assertTrue(flow.matches(5));
    }
}
