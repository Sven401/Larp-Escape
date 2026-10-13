using Android.Bluetooth;
using Android.OS;
using System.Buffers.Binary;
using System.Text;
using System.Text.Json;

namespace HandyEspDiagnostic;

// Contract lab only. No peer authentication, profile editor or physical-owner claim.
sealed class V2Session(BluetoothGatt gatt, Action<string> report, Action<uint,string> complete,
    uint previousLease, string? previousBoot)
{
    const string Prefix = "86ddc5b0-3878-4ff7-9f55-17c62aeb";
    BluetoothGattCharacteristic command = null!, output = null!, marker = null!;
    readonly Dictionary<byte, Assembly> assemblies = new();
    readonly Handler timer = new(Looper.MainLooper!);
    byte[]? sending;
    int offset, stage;
    ushort message;
    uint lease, cfg, state, request;
    string boot = "", expected = "", firstCommand = "";
    JsonElement firstReply;
    bool stopped, disposed;
    int replyGeneration;
    long writeStarted;
    sealed class Assembly { public ushort Message; public byte[] Bytes = []; public int Offset; public long Started; }
    public void Dispose() { disposed = true; timer.RemoveCallbacksAndMessages(null); assemblies.Clear(); }
    void Check(bool ok, string why) { if (!ok) throw new InvalidOperationException(why); }
    void Guard(Action action) { if(disposed || stopped)return;try { action(); }catch(Exception e) { Fail(e.Message); } }
    void Fail(string why) { stopped = true; report("V2 VERTRAG FAIL: " + why); }
    public void Start(BluetoothGattService service) => Guard(() => {
        marker = service.GetCharacteristic(Java.Util.UUID.FromString(Prefix+"1002"))!;
        command = service.GetCharacteristic(Java.Util.UUID.FromString(Prefix+"1003"))!;
        output = service.GetCharacteristic(Java.Util.UUID.FromString(Prefix+"1004"))!;
        Check(marker != null && command != null && output != null,"Labendpunkte fehlen");
        Check(gatt.ReadCharacteristic(marker),"Labmarker nicht lesbar");
    });
    public void Read(BluetoothGattCharacteristic? characteristic, byte[]? bytes, GattStatus status) => Guard(() => {
        if(characteristic!=marker)return;
        Check(status==GattStatus.Success && bytes!=null && Encoding.UTF8.GetString(bytes)=="V2 LAB; fake owners; injected authority; RAM journal","Labmarker ungültig");
        report("V2-Vertragslabor: echte Codecs/Befehlslogik; Geräte, Journal und Berechtigung sind Testeingänge.");
        Check(gatt.SetCharacteristicNotification(output,true),"Notifications nicht aktiviert");
        var descriptor=output.GetDescriptor(Java.Util.UUID.FromString("00002902-0000-1000-8000-00805f9b34fb"));
        Check(descriptor!=null && gatt.WriteDescriptor(descriptor,new byte[]{1,0})==0,"Abonnement nicht gestartet");
    });
    public void Descriptor(GattStatus status) => Guard(() => {
        Check(status==GattStatus.Success,"Abonnement fehlgeschlagen");
        report("V2-Fragmente abonniert; warte auf vollständigen Status.");
        timer.PostDelayed(()=> { if(stage==0&&!disposed&&!stopped)Fail("Kein vollständiger Anfangsstatus in 15 s"); },15000);
    });
    public void Changed(BluetoothGattCharacteristic? characteristic, byte[] bytes) => Guard(() => {
        if(characteristic!=output)return;
        Check(bytes.Length>=9 && bytes.Length<=128 && bytes[0]==1 && (bytes[1]==2||bytes[1]==4),"Frameversion/-typ/-länge");
        byte kind=bytes[1];ushort id=BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(2));
        ushort total=BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(4)), at=BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(6));
        int payload=bytes.Length-8, limit=kind==2?511:6143;long now=SystemClock.ElapsedRealtime();
        Check(id>0 && total>0 && total<=limit && at+payload<=total,"Framegrenzen");
        if(!assemblies.TryGetValue(kind,out var a) || a.Message!=id) {
            // Subscription may start midway through an already frozen status.
            if(at!=0 && kind==4)return;
            Check(a==null,"Neue Nachricht ersetzt unvollständige Nachricht");
            Check(at==0,"Antwort beginnt nicht bei Offset 0");
            a=new Assembly{Message=id,Bytes=new byte[total],Started=now};assemblies[kind]=a;
        }
        Check(a.Bytes.Length==total && a.Offset==at && now-a.Started<(kind==2?2000:30000),"Fragmentreihenfolge/absolute Frist");
        bytes.AsSpan(8).CopyTo(a.Bytes.AsSpan(at));a.Offset+=payload;
        if(a.Offset!=total)return;assemblies.Remove(kind);
        using var document=JsonDocument.Parse(a.Bytes);var json=document.RootElement.Clone();
        Check(json.GetProperty("schemaVersion").GetInt32()==2,"JSON-Version");
        report((kind==2?"V2 REPLY ":"V2 STATUS ")+Encoding.UTF8.GetString(a.Bytes));
        if(kind==4) Status(json);else Reply(json);
    });
    void Status(JsonElement json) {
        var application=json.GetProperty("application");var control=json.GetProperty("control");var game=json.GetProperty("game");
        if(stage==0) {
            Check(application.GetProperty("phase").GetString()=="ready" && application.GetProperty("hasActive").GetBoolean()
                && application.GetProperty("hardwareReady").GetBoolean() && application.GetProperty("storedMatchesActive").GetBoolean(),"Profil nicht bereit");
            boot=json.GetProperty("boot").GetString()!;Check(boot.Length==16 && Convert.ToUInt64(boot,16)!=0,"Bootkennung");
            lease=control.GetProperty("lease").GetUInt32();cfg=application.GetProperty("configurationRevision").GetUInt32();state=game.GetProperty("stateRevision").GetUInt32();
            Check(lease>0 && control.GetProperty("nextRequest").GetUInt32()==1 && !game.GetProperty("running").GetBoolean() && json.GetProperty("devices").GetArrayLength()>0,"Anfangsstatus");
            if(previousLease!=0 && previousBoot==boot) { Check(lease!=previousLease,"Lease nach Reconnect unverändert");stage=9;Send(1,"stop",cfg,state,previousLease,"unauthorized"); }
            else {stage=1;Send(1,"start",cfg,state,lease,"executed");}
        } else if(stage==10 && control.GetProperty("nextRequest").GetUInt32()==5) {
            Check(json.GetProperty("boot").GetString()==boot && control.GetProperty("lease").GetUInt32()==lease && !game.GetProperty("running").GetBoolean(),"Stopstatus/Korrelation");
            stopped=true;complete(lease,boot);report("V2 VERTRAG PASS: vollständiger Status, Start/Stop, Wiederholung, Konflikt, Revisionen, Bootkennung und Reihenfolge über BLE. Für Lease-Prüfung erneut verbinden.");
        }
    }
    string Build(uint number,string kind,uint configuration,uint revision,uint commandLease,string? commandBoot=null) =>
        JsonSerializer.Serialize(new {schemaVersion=2,boot=commandBoot??boot,lease=commandLease,request=number,expectedConfigurationRevision=configuration,expectedStateRevision=revision,kind,target=0});
    void Send(uint number,string kind,uint configuration,uint revision,uint commandLease,string outcome,string? commandBoot=null) => SendJson(Build(number,kind,configuration,revision,commandLease,commandBoot),number,outcome);
    void SendJson(string json,uint number,string outcome) {
        Check(sending==null,"Überlappende Schreibaufträge");request=number;expected=outcome;
        if(stage==1)firstCommand=json;
        sending=Encoding.UTF8.GetBytes(json);offset=0;++message;writeStarted=SystemClock.ElapsedRealtime();
        Check(message>0 && sending.Length<=1023,"Commandgrenzen");
        report($"V2 SEND stage={stage} request={number} expected={outcome}");WriteNext();
        int token=++replyGeneration;timer.PostDelayed(()=> { if(token==replyGeneration&&!disposed&&!stopped)Fail("Antwort fehlt nach 5 s"); },5000);
    }
    void WriteNext() {
        if(sending==null)return;int count=Math.Min(12,sending.Length-offset);var frame=new byte[8+count];frame[0]=1;frame[1]=1;
        BinaryPrimitives.WriteUInt16LittleEndian(frame.AsSpan(2),message);BinaryPrimitives.WriteUInt16LittleEndian(frame.AsSpan(4),(ushort)sending.Length);BinaryPrimitives.WriteUInt16LittleEndian(frame.AsSpan(6),(ushort)offset);
        sending.AsSpan(offset,count).CopyTo(frame.AsSpan(8));offset+=count;
        Check(gatt.WriteCharacteristic(command,frame,(int)GattWriteType.Default)==0,"GATT-Schreiben nicht gestartet");
    }
    public void Written(BluetoothGattCharacteristic? characteristic,GattStatus status) => Guard(() => {
        if(characteristic!=command)return;Check(status==GattStatus.Success && sending!=null && SystemClock.ElapsedRealtime()-writeStarted<2000,"Command-Schreiben/absolute Frist");
        if(offset<sending!.Length)WriteNext();else sending=null;
    });
    void Reply(JsonElement json) {
        Check(sending==null,"Antwort vor letztem Schreibcallback");
        Check(json.GetProperty("boot").GetString()==boot && json.GetProperty("request").GetUInt32()==request
            && json.GetProperty("lease").GetUInt32()==(stage==9?previousLease:lease) && json.GetProperty("outcome").GetString()==expected,"Antwortkorrelation/Ergebnis");
        bool consumed=stage is 1 or 2 or 4 or 7 or 8;
        Check(json.GetProperty("consumed").GetBoolean()==consumed && json.GetProperty("duplicate").GetBoolean()==(stage==2),"Verbrauch/Wiederholung");
        uint next=stage switch {1 or 2 or 3=>2,4 or 5 or 6=>3,7=>4,8=>5,9=>1,_=>0};
        Check(json.GetProperty("nextRequest").GetUInt32()==next,"Nächste Anfragenummer");++replyGeneration;
        cfg=json.GetProperty("configurationRevision").GetUInt32();state=json.GetProperty("stateRevision").GetUInt32();
        switch(stage) {
            case 1:firstReply=json;stage=2;SendJson(firstCommand,1,"executed");break;
            case 2:Check(state==firstReply.GetProperty("stateRevision").GetUInt32() && cfg==firstReply.GetProperty("configurationRevision").GetUInt32() && json.GetProperty("executionOutcome").GetString()==firstReply.GetProperty("executionOutcome").GetString(),"Replay verändert Originalantwort");stage=3;Send(1,"pause",cfg,state,lease,"request_conflict");break;
            case 3:stage=4;Send(2,"pause",uint.MaxValue,state,lease,"stale_configuration");break;
            case 4:stage=5;Send(3,"pause",cfg,state,lease,"wrong_boot",boot=="0000000000000001"?"0000000000000002":"0000000000000001");break;
            case 5:stage=6;Send(4,"pause",cfg,state,lease,"out_of_order");break;
            case 6:stage=7;Send(3,"pause",cfg,uint.MaxValue,lease,"stale_state");break;
            case 7:stage=8;Send(4,"stop",cfg,state,lease,"executed");timer.PostDelayed(()=> {if(!stopped&&!disposed)Fail("Vollständiger Stopstatus fehlt nach 10 s");},10000);break;
            case 8:stage=10;break;
            case 9:stopped=true;complete(lease,boot);report("V2 RECONNECT PASS: neue Lease; alte Lease als unauthorized ohne Verbrauch abgewiesen.");break;
        }
    }
}
