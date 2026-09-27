# Regras de ProGuard/R8 do módulo app.
# JNI: mantém os métodos nativos e as classes que os declaram para não quebrar o binding.
-keepclasseswithmembernames class * {
    native <methods>;
}
