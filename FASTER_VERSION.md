# EMUZ80 kuma version

Norihiro KumagaiによるEMUZ80.  オリジナルは電脳伝説さん(@vintagechips)の[EMUZ80](https://github.com/vintagechips/emuz80)です。

ここでは高速版(ブランチfaster)について説明します。

## EMUZ80高速版

* EMUZ80は、Z80 CPUをマイクロコントローラPIC18F47Q43で駆動する試みです。
* PIC18F47Q43のCLC(Configurable Logic Cell)を使い、Z80のメモリサイクルを待たせる/WAIT信号発生を外部回路なしで実現しており、わずか2チップでMicrosoft BASICを起動します。
* 高速版は、従来のソフトウェアループ版に加え、ピンアサインを変更し、アドレスデコードとWAIT待ちを1命令で実現し、またアセンブリ言語で記述することで高速化を狙いました。

## 取り組みの趣旨(Purpose)

### アドレスデコード

EMUZ80のメモリマップは以下の通り(see https://github.com/hlide/emuz80-hlide/blob/main/emuz80.X/main.c)

* ROM: 8kB ($0000 - $1FFF)
* RAM: 4kB ($8000 - $8FFF)
* I/O: $E000 (data) / $E001 (status)

```
 * Memory mapping to ease address decoding:
 * 
 * +------------------------------------+ $0000
 * |                                    |
 * :              ROM 8KB               : RO(A15..13 = 000b) => ROM
 * :                                    : WR(A15..13 = 000b) => no storing
 * |                                    | $1FFF
 * +------------------------------------+ $2000
 * |                                    |
 * :              GARBAGE               : RO(A15..13 = 001b) => GARBAGE
 * :                                    : WR(A15..13 = 001b) => no storing
 * |                                    | $7FFF
 * +------------------------------------+ $8000
 * |                                    |
 * :              RAM 4KB               : RW(A15..13 = 100b) => RAM
 * |                                    | $8FFF
 * +------------------------------------+ $9000
 * |                                    |
 * :          RAM 4KB mirrored          : RO(A15..14 != 100b) => RAM mirrored
 * :                                    : WO(A15..14 != 100b) => no storing
 * |                                    |
 * +------------------------------------+ $BFFF-$C000
 * |                                    |
 * :        I/O registers               : RW(A14 = 1b) => UART
 * |[--------U3TXB/U3RXB---------] $E000| 
 * |[-----------PIR9-------------] $E001|
 * |                                    |
 * +------------------------------------+ $FFFF
```


となっている。ここでは、上位3bit (A13,A14,A15)に着目して、上記以外のアドレスはアクセスされないと想定すると、以下の真偽表を得る。

|A15|A14|A13|action|
|---|---|---|---|
|0|0|0|ROM|
|0|0|1|garbage|
|0|1|0|garbage|
|0|1|1|garbage|
|1|0|0|RAM|(A12が1の時は無効アクセス)
|1|0|1|garbage|
|1|1|0|garbage|
|1|1|1|I/O(下位8ビットが0, 1のみ)|


これだけだと面白味はないが、PIC18Fの機械語に「PCにアキュムレータ(Wレジスタ)の内容を足す」というものがある。`ADDWF PCL`である。アキュムレータに上記の3ビットの値を足すことを想像してみると、この命令の後ろにジャンプ命令を並べておくと、アドレスにより飛び先を変えてジャンプする処理を2命令で実行できることが分かる。

実際には、`/WAIT`、`/RD`も加えて5ビットで32エントリのジャンプテーブルを構成する。GPIOから読み込んだ値を直接PCに足せるように、/WAiT, /RD, A15, A14, A13をGPIOのいずれかのレジスタの下位5ビットに割り当てる。RA0-4に割り当てるために、ハードウェアを変更する。

|RA4<br>/WAIT|RA3<br>/RD|RA2<br>A15|RA1<br>A14|RA0<br>A13|action|
|---|---|---|---|---|---|
|0|0|0|0|0|READ_ROM|
|0|0|0|0|1|do nothing|
|0|0|0|1|0|do nothing|
|0|0|0|1|1|do nothing|
|0|0|1|0|0|READ_RAM(A12が1の時はゴースト読み込み)|
|0|0|1|0|1|do nothing|
|0|0|1|1|0|do nothing|
|0|0|1|1|1|READ UART(下位8ビットが0, 1のみ)|
|0|1|0|0|0|do nothing|
|0|1|0|0|1|do nothing|
|0|1|0|1|0|do nothing|
|0|1|0|1|1|do nothing|
|0|1|1|0|0|WRITE_RAM(A12が1の時は無効アクセス)|
|0|1|1|0|1|do nothing|
|0|1|1|1|0|do nothing|
|0|1|1|1|1|WRITE_UART(下位8ビットが0, 1のみ)|
|1|X|X|X|X|goto loop_top|

PORTAからの読み込み後、ベクタサイズ2バイト単位のオフセットとするために、読み込んだ値を2倍する。幸いなことに読み込みと2倍を1命令で実行できる`RINCF PORTA,W`ので無駄がない。

とりあえず見よう見まねでアセンブリ言語版を書き下ろしてみよう。まだビルドも通していないが、ステップ数はおおむねこんなものだろう。

```
loop_top
    RINCF PORTA,W       ; W <- PORTA * 2
    ANDWF 3EH           ; W &= 0x3f
    ADDWF PCL           ; PC += W
    GOTO  READ_ROM      ;00000b
    GOTO  DO_NOTHING    ;00001b
    GOTO  DO_NOTHING    ;00010b
    GOTO  DO_NOTHING    ;00011b
    GOTO  READ_RAM      ;00100b
    GOTO  DO_NOTHING    ;00101b
    GOTO  DO_NOTHING    ;00110b
    GOTO  READ_UART     ;00111b
    GOTO  DO_NOTHING    ;01000b
    GOTO  DO_NOTHING    ;01001b
    GOTO  DO_NOTHING    ;01010b
    GOTO  DO_NOTHING    ;01011b
    GOTO  WRITE_RAM     ;01100b
    GOTO  DO_NOTHING    ;01101b
    GOTO  DO_NOTHING    ;01110b
    GOTO  WRITE_UART    ;01111b
    GOTO  loop_top      ;10000b
    GOTO  loop_top      ;10001b
    ...
    GOTO  loop_top      ;11111b
READ_ROM:
    MOVFF PORTB,tblprtl
    MOVFF PORTD,tblprth
    TBLRD *
    MOVFF TABLAT,LATC
    MOVFF 0,TRISB       ; D0_7_dir = 0x00 (output)
DO_NOTHING:
    MOVFW (1<<4)
    ORWF LATA
    MOVWF LATA          ; LATA4 |= 1
    MOVWF ~(1<<4)
    ANDWF LATA
    MOVWF LATA          ; LATA4 &= ~1
    MOVWF PORTA
    ANDFW (1<<0)        ; PORTA &= (1<<0)
    BZ    loop_tail
    MOVFF 0xff,TRISB    ; D0_7_dir = 0xff (input)
    GOTO  loop_top
READ_RAM:
    ...
    GOTO  loop_top
WRITE_RAM:
    ...
    GOTO  loop_top
READ_UART
    ...
    GOTO  loop_top
WRITE_UART
    ...
    GOTO  loop_top
```

READ_ROMの実行は、loop_topから最後の`GOTO loop_top`まで、21命令、1命令62.5nsとして、約1312ns, 1.2us、T1の下りエッジでMREQが落ちると同時に/WAITが落ち、それをソフトで検出してから1312us、Z80のクロックが2.5MHz(400ns)として、1312usは3クロック強、うまく行けば2WAITで実行できそうな感触を得る。

## ハードウェアの変更

以下のピンが変更対象である。

|ピン名|変更前|変更後|
|--|--|--|
|A15|RD7|RA2|
|A14|RD6|RA1|
|A13|RD5|RA0|
|/RD|RA5|RA3|
|CLK|RA3|RD7|
|/RFSH|RA2|RD6|
|/MREQ|RA1|RD5|
|/IOREQ|RA0|NC|



githubの回路図どおりのハードウェアを用意します。手元に用意したハードには以下の変更を加えました。

* 2.1Φの電源アダプタジャックは搭載していません。
* シリアルの受信側(キーボード入力が来るほう)をUSBシリアルアダプタと切り離せるようにした。

<figure>
    <img height=300 src="imgs/HW_SerialMod.jpg" caption="XXXXXX">
    <figcaption>USBシリアル接続基板の回路図</figcaption>
</figure>

<figure>
    <img height=300 src="imgs/HW_SerialModPic.jpg" caption="XXXXXX">
    <figcaption>USBシリアル接続基板</figcaption>
</figure>

これは、TESTピンを用意するためです。

ソフト側でON/OFFできるピンを1つ用意し、そのピンをソフトでON/OFFすることで、ロジックアナライザからソフトの処理時間と外部信号のタイミングを読み取ることができるようにします。

EMUZ80では、PICの40ピンすべて使い切っており未使用GPIOピンはありません。よって、今割り当て済だがなくてもなんとかなるピンを空けてTESTピンとして使用することを考えました。

Z80のメモリアクセスサイクルを追い込むときに、シリアル入力はなくてもなんとかなります。この追い込みでは、
* 無限NOP地獄…リードサイクルで0しか返さない、M1サイクルのタイミングを見る
* 無限01地獄…LXI B,XXXX命令を無限に繰り返す、メモリリードサイクルを見る
* 無限RST0地獄…リードサイクルで0xC7しか返さない、メモリライトサイクルのタイミングを見るために用いる

を使うつもりなので、シリアル入力は使用しないのです。最終的に、BASICインタプリタ起動を確認した時点でTESTピン出力を殺し、UART3のRX入力を復活させるようにします。

この基板の役割は、USBシリアルアダプタとRA7を分離することで、ロジックアナライザのプローブはPICそばの穴にピンを立てて、そこを取り付けます。

### MPLABXビルド環境の整備

Windows版をインストールし、ビルド・ターゲットの書き込みもWindows版IDEで行います。

ビルドしたHEXファイルの書き込みは、MPLAB Snapを使います。

MPLAB XもPIC18F47Q43も初挑戦なので、あらかじめ、スクラッチからプロジェクトを起こし、Lチカができるところまでを体験しておきました。

### githubリポジトリからのダウンロード

```
$ git clone https://github.com/vintagechips/emuz80.git
```
　手元にチェックアウトしておきます。ソースファイルは`main.c`1本だけなので簡単です。

> その後、自分のリポジトリを起こしました。https://github.com/tendai22/emuz80_kuma.git を参照ください。というかこのファイルを見ているということは、このリポジトリを見つけているはず。

### ビルドしてみる。

まっさらのプロジェクトを`PIC18F47Q43`, `Snap`で作り、空の`main.c`をチェックアウトした`main.c`を入れ替えます。これでビルドしてダウンロード、実行できます。

### TEST Pinを活かす

main関数内、初期化部分の最後に、RA7をTESTピンにする設定を書いてあります。

```
//#define TEST_PIN A7
#if defined(TEST_PIN)
    // RA7 as TEST pin ()
    TRISA7 = 0;     // A7 output
    LATA7 = 0;      // pin value is 0
    RA7PPS = 0;     // PPS as LATA7
#else
#undef TOGGLE
#define TOGGLE()
#endif //defined(TEST_PIN)
```

マクロ`TEST_PIN`を定義するとRA7はGPIO出力となり、UART3のRXとRA7は切り離されます。ハード的にRXとUSBシリアルのTXとは切り離しておいてください。つないだままだと地獄の謎データでTeraTerm画面が埋まってしまいます。

> TEST_PIN の値として`A7`を定義していますが、この値はまったく参照されません。気持ちだけです。

マクロ`TOGGLE()`をコード中で呼び出すと、そこでRA7が反転します。

現在のところ、/WAIT待ちループを抜け出たところと、/WAITを戻しデータバスを入力に切り替えた直後に1個ずつ入れてあります。ロジックアナライザでRA7を見ると、内部処理の開始と終了を知ることができます。

### 処理の概要

* /WAITピンがLになるまで待つ。Lになれば処理を開始する。
* CLCにより、/WAITピンがLになるということはメモリリードまたはライトサイクルであることは保証されている。
* /RD(RA5)がLならリードサイクルである。アドレスを読み込み、ROM/RAM/UART_CREG/UART_DREGの値をデータバスに出力する。
* /RDがHならライトサイクルである。/WRピンがLになるまで待ち、データバスからデータを読み込みRAM/UART_DREGに書き込む。
* 処理が終わると/WAITを戻す(Hを出力する)。/MREQがHに戻るのを待ち、すかさずデータバスを入力にする。

### 実行時間の比較

オリジナルの割り込み版とソフトループ版の比較。リードサイクルとライトサイクルを/MREQ==Lの間の時間で比較する。

以下のロジックアナライザ写真の信号割り当ては以下のとおりである。TEST Pin設定していないので、信号は常時Hである。
|||
|--|--|
|0(灰色)|Z80 CLK|
|1(茶色)|TEST Pin|
|2(赤色)|Z80 MREQ|
|3(橙色)|Z80 RD|
|4(黄色)|Z80 WR|
|5(緑色)|Z80 RESET|


### 割り込み版

オリジナルではZ80 CPUに供給されるクロック周波数は2.5MHzである。
M1サイクルで3.4us, メモリライトサイクルで3.18usであった。

<figure>
    <img height=300 src="imgs/INT_M1cycle.png" caption="XXXXXX">
    <figcaption>割り込み版 M1サイクル(READ)</figcaption>
</figure>

<figure>
    <img height=300 src="imgs/INT_MWcycle.png" caption="XXXXXX">
    <figcaption>割り込み版 メモリライトサイクル</figcaption>
</figure>

### ソフトループ版

クロックは2.0MHzに落としてある。
M1サイクルで2.7us, メモリライトサイクルで2.5usであった。若干速くなっている。

<figure>
    <img height=300 src="imgs/POLL_M1cycle_2MHz.png" caption="XXXXXX">
    <figcaption>ソフトループ版 M1サイクル(READ)</figcaption>
</figure>

<figure>
    <img height=300 src="imgs/POLL_MWcycle_2MHz.png" caption="XXXXXX">
    <figcaption>ソフトループ版 メモリライトサイクル</figcaption>
</figure>

### メモリサイクル終了時の処理

/WAITをHにするとZ80は実行を再開します。まず/MREQをHに戻し次のサイクルに入ります。PIC側では、次のサイクルが始まる前にデータバスを入力に戻しておかねばなりません。戻さないと、次のサイクルがライトサイクルの場合、データバスで出力が衝突する可能性があります。

PICが/WAITをHに戻してからどれぐらいの時間でZ80 CPUが/MREQを戻すかが読みにくいです。王道は/MREQを監視してHに戻ってからデータバスを入力に戻す(`TRISC=0xff`)のですが、Z80クロックが速くなってくると/MREQがHになったことを検知してからTRISCに代入する前に、Z80 CPUが次のメモリサイクルを開始してしまうことが生じます。

<figure>
    <img height=300 src="imgs/POLL_BUSCHANGE_2MHz.png" caption="XXXXXX">
    <figcaption>ソフトループ版 メモリリードサイクル終了点(クロック2MHz)</figcaption>
</figure>

クロック2.0MHzでのMREQ==Hから`TRISC=0xff`までの時間を表しています。波形1はTEST Pinであり、ここではリードサイクル最後に`TRISC=0xff`した直後にLに落としています。次にデータバスが出力状態になる場合の、基準時間はクロックT1の下りエッジから規定されています。最大値は60nsと決まっているのですが、最小値がいくらになるかの規定はありません。最悪を考えるとT1の下りエッジと同時に出力状態になることを想定します。よって、クロックT1の下りエッジより前にTEST Pinの下りエッジが来るようにすれば絶対安全です。

上記のロジックアナライザ波形では、クロックT1の下りエッジより前にTEST Pinの下りエッジがあるのでOKです。

クロック2.5MHzに上げると、TEST Pinの下りエッジが遅くなり、T1の下りエッジより後になってしまいます。
<figure>
    <img height=300 src="imgs/POLL_BUSCHANGE_EXP_2.5MHz.png" caption="XXXXXX">
    <figcaption>ソフトループ版 メモリリードサイクル終了点(クロック2.5MHz)</figcaption>
</figure>

40nsの遅延で厳密にはよろしくないのですが、実際のところは、
TEST PinがH->Lに設定するより60ns前に`TRISC=0xff`を実行している
ため、40ns遅延はぎりぎりOKと考えます。

ということで、上記2MHzで時間を測定しましたが、2.5MHzで再度測定すべきかもしれません。








