# The Espressif SDK logs the raw QR and PoP. Strip every Android log call
# (including throwable overloads) from the delivered, optimized release APK.
-assumenosideeffects class android.util.Log { public static *** *(...); }
-assumenosideeffects class java.lang.Throwable { public void printStackTrace(...); }
-keepattributes *Annotation*,Signature,InnerClasses,EnclosingMethod
-keepclassmembers class * { @org.greenrobot.eventbus.Subscribe <methods>; }
-keep class com.google.protobuf.** { *; }
-keep class espressif.** { *; }
