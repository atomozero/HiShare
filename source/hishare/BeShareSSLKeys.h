// Auto-generated: BeShare's self-signed RSA-2048 key pair for opportunistic TLS on
// peer-to-peer file transfers.  The private key ships in the binary, so this provides
// ENCRYPTION against passive eavesdropping, NOT authentication/anti-MITM.  Regenerate
// with: openssl req -x509 -days 3650 -nodes -newkey rsa:2048 -keyout p.pem -out c.pem
#ifndef BeShareSSLKeys_h
#define BeShareSSLKeys_h

// Private key (PEM) — used by the UPLOADER (TLS server) via SetSSLPrivateKey().
static const char * const kBeShareTLSPrivateKeyPEM =
   "-----BEGIN PRIVATE KEY-----\n"
   "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCdvx42fd8nop9y\n"
   "nF8CpXoCJjdjz4qhEdouFR/BfGbwrXA2TtgX6+Cz5MSqOTUI/NaUZYOzR1IKCvSk\n"
   "KLqbeF6zpkIzFU0CxymqCzycZ0+DEaIEiyL7WIF3liwXq+Y28Eig57Ei8P2o3vru\n"
   "VdovaqkSOOxS6OofxU5KUficELw35mIqdT5chg54fPwJYLWQ+8FsfYSXV5RqrLts\n"
   "XxCpbY1mOGdHQJi/o+e5L4nL8I18YMwbIUscVoA236iSfaTTTCWnNHMk5muzYMaD\n"
   "3IiDEsgmn7WqbRQAEcA+bTVgaUu10HfwnQ59RiNy/NsmXZU20xv5WJrj6KX6UCz3\n"
   "lvnrwrI9AgMBAAECggEAEKSh6Ojl2vYu8XYrFgnkcgabMHYsr6rFBLio1Y6tA7ag\n"
   "0LIH3Zo+5mv8DeOkQ1L9xsFhIFCliLa4Mfu0GIQeJcEkWeRk8CaYMD8oQW4q9u6s\n"
   "0+LkKXWgjz/yXfr+eoxmdUeMql2XvbHs8qlvMDKGLQyBbovxa7Gy/WdhngGsAg+r\n"
   "olrH8mxM3w3hX8wXxaoRP+1+eDnthUcAbJfglQBgElfXIZcqaOMeOqCbTY/rMrc9\n"
   "+piPRRl/nVG0i0Ob2ha1lJytRplAAyKIP99Ur+AnXSEpegLssvghilUJoubyph+l\n"
   "nHowpkcEUb7ngy5mvc4TNHdUqyfJ37frF6DPfmjisQKBgQDLizh1kKTn3ZmY/sbG\n"
   "0qD5mAQHkPfZG76bzfLtjXf1ctKDQYEhqfcyKoCma+7IoQTk957gy6Tl0gYHm54f\n"
   "c34g9iGptiAWKEzJH3WWDfI75r9AArAXa03pHXdLhd3d28/togIlRYNbbx9YTzDb\n"
   "Z5FOBhwAgsb0XMKTZ8JBXFyEuQKBgQDGZm2Dy5e41dbNLn83q66fsk+siV4OdLGG\n"
   "39BC18zm4ImoELInuPdmbcG3oBYtiUegDy6ZRhhV9mWQkGLSMOS+nM/aLPuWrBCD\n"
   "jQhZd9qxogaI07nZsLSkS/Exs0cn1lNotBgjiADiiGbSfmUr1bS5u9RbpLOMIPUu\n"
   "NcZG9qTfpQKBgHjXAoHpib2ONi46s9A+JAuKUHodeseOTS86qcqWfm1d6dS7Aur1\n"
   "eShzS0WCNpUt4zX8PHrA6/j1I1dI8CtD6dGvznRvB8Wfz1ZoMusPBIzDhS9/aQO+\n"
   "VNbdA4H2y613Xo557EQsbLvP0RAgk6Tua7miruUuvuc8WtggdZdQOi0hAoGBAJT4\n"
   "afjREU5XOl98L0fk4EzG/a4mwvoFwxrryRu3oAInzRTl3G+ZIiLF/PPc/2oXrzFO\n"
   "1QV/rVw2k9J1p839qnQ5mJRQvGRJNGkip4dOSGaiq1dn6x+64BiRcTyJSb9u3dg7\n"
   "ifh78XYnR1V8VIkSgQ4JLA3X4H0ybgbRl9zG68uFAoGBAIpc2BfMnvGYPo8VMMVD\n"
   "UlhHAPLWxyfKERfZWIvkJMe18CTpQ48FdPpYajq6SP00+FD55X01sd9mQYnsNmqk\n"
   "lfZmbyjxLy6CsQqwVCheWWqxZMVxk6rwqNAdJDDumKz48ct3QgFiBvgbCtG2GdAr\n"
   "VVxLfz77o2kpo2/Ao1ljukdf\n"
   "-----END PRIVATE KEY-----\n"
   "-----BEGIN CERTIFICATE-----\n"
   "MIIDHzCCAgegAwIBAgIUYqjtJDnh66OvYeHa/SR6JAmO2dEwDQYJKoZIhvcNAQEL\n"
   "BQAwHzEdMBsGA1UEAwwUQmVTaGFyZSBQMlAgVHJhbnNmZXIwHhcNMjYwNzA0MTg0\n"
   "OTUwWhcNMzYwNzAxMTg0OTUxWjAfMR0wGwYDVQQDDBRCZVNoYXJlIFAyUCBUcmFu\n"
   "c2ZlcjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ2/HjZ93yein3Kc\n"
   "XwKlegImN2PPiqER2i4VH8F8ZvCtcDZO2Bfr4LPkxKo5NQj81pRlg7NHUgoK9KQo\n"
   "upt4XrOmQjMVTQLHKaoLPJxnT4MRogSLIvtYgXeWLBer5jbwSKDnsSLw/aje+u5V\n"
   "2i9qqRI47FLo6h/FTkpR+JwQvDfmYip1PlyGDnh8/AlgtZD7wWx9hJdXlGqsu2xf\n"
   "EKltjWY4Z0dAmL+j57kvicvwjXxgzBshSxxWgDbfqJJ9pNNMJac0cyTma7NgxoPc\n"
   "iIMSyCaftaptFAARwD5tNWBpS7XQd/CdDn1GI3L82yZdlTbTG/lYmuPopfpQLPeW\n"
   "+evCsj0CAwEAAaNTMFEwHQYDVR0OBBYEFAiZ2P5KUwjrcJ89zBMc+7qfGO7ZMB8G\n"
   "A1UdIwQYMBaAFAiZ2P5KUwjrcJ89zBMc+7qfGO7ZMA8GA1UdEwEB/wQFMAMBAf8w\n"
   "DQYJKoZIhvcNAQELBQADggEBAHV6nh12BUyG0+X2W6BjZXCCDWmXmR9wn7wAjIAY\n"
   "7XyDKt2LaheP1dgzYaX5KHzaTliE4yz/crIuc0Lk7z46t3e2ljxf2B0yPX6oVlNV\n"
   "Tb57wpfJm9qcA3VFsRbhkpfRZz+g95EIuv8Bu0jS3zAsu414c25Jqf9ZSxZvlU97\n"
   "+CcK9/mtEJ4b/9JRxbY/h7cwyemqWxOzM4h085dYeBa59/mZRiR+XVXSGLgDJrl2\n"
   "OrPaWVatlQ1FyFC176d38kEDHj6oSaXt7iKTTPfrhAQxbNnD8UuxxH3rA268wFGJ\n"
   "T7oUZHeKwRt87snO9smlr64KpH6TpZELMnqZ215wNdZK7+U=\n"
   "-----END CERTIFICATE-----\n"
   ;

// Public certificate (PEM) — used by BOTH sides via SetSSLPublicKeyCertificate().
static const char * const kBeShareTLSPublicKeyPEM =
   "-----BEGIN CERTIFICATE-----\n"
   "MIIDHzCCAgegAwIBAgIUYqjtJDnh66OvYeHa/SR6JAmO2dEwDQYJKoZIhvcNAQEL\n"
   "BQAwHzEdMBsGA1UEAwwUQmVTaGFyZSBQMlAgVHJhbnNmZXIwHhcNMjYwNzA0MTg0\n"
   "OTUwWhcNMzYwNzAxMTg0OTUxWjAfMR0wGwYDVQQDDBRCZVNoYXJlIFAyUCBUcmFu\n"
   "c2ZlcjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ2/HjZ93yein3Kc\n"
   "XwKlegImN2PPiqER2i4VH8F8ZvCtcDZO2Bfr4LPkxKo5NQj81pRlg7NHUgoK9KQo\n"
   "upt4XrOmQjMVTQLHKaoLPJxnT4MRogSLIvtYgXeWLBer5jbwSKDnsSLw/aje+u5V\n"
   "2i9qqRI47FLo6h/FTkpR+JwQvDfmYip1PlyGDnh8/AlgtZD7wWx9hJdXlGqsu2xf\n"
   "EKltjWY4Z0dAmL+j57kvicvwjXxgzBshSxxWgDbfqJJ9pNNMJac0cyTma7NgxoPc\n"
   "iIMSyCaftaptFAARwD5tNWBpS7XQd/CdDn1GI3L82yZdlTbTG/lYmuPopfpQLPeW\n"
   "+evCsj0CAwEAAaNTMFEwHQYDVR0OBBYEFAiZ2P5KUwjrcJ89zBMc+7qfGO7ZMB8G\n"
   "A1UdIwQYMBaAFAiZ2P5KUwjrcJ89zBMc+7qfGO7ZMA8GA1UdEwEB/wQFMAMBAf8w\n"
   "DQYJKoZIhvcNAQELBQADggEBAHV6nh12BUyG0+X2W6BjZXCCDWmXmR9wn7wAjIAY\n"
   "7XyDKt2LaheP1dgzYaX5KHzaTliE4yz/crIuc0Lk7z46t3e2ljxf2B0yPX6oVlNV\n"
   "Tb57wpfJm9qcA3VFsRbhkpfRZz+g95EIuv8Bu0jS3zAsu414c25Jqf9ZSxZvlU97\n"
   "+CcK9/mtEJ4b/9JRxbY/h7cwyemqWxOzM4h085dYeBa59/mZRiR+XVXSGLgDJrl2\n"
   "OrPaWVatlQ1FyFC176d38kEDHj6oSaXt7iKTTPfrhAQxbNnD8UuxxH3rA268wFGJ\n"
   "T7oUZHeKwRt87snO9smlr64KpH6TpZELMnqZ215wNdZK7+U=\n"
   "-----END CERTIFICATE-----\n"
   ;

#endif
