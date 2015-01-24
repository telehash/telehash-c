#include "e3x.h"
#include "util.h"
#include "unit_test.h"
#include "util_sys.h"

// fixtures
#define A_KEY "gcbaccqcqiaqcagucsudlfbqcf2kaivyacngcba2p5vrkejpjdnelitvsdmuh26wm7zk7eycklcko6ej57l2tlo3izytz32sqqgfof4l2aj52x6xf2vnodn6k6f5saocjybmfuemvebrccdzbekbm3sknk3j54y434mkqt245p46ahmusw7tux3ajllsye54rjog4gb4eyamxixcc5azno652z67556gd6wilmd7lp5hc4rxa7hwiw2t6usxjamcejnzolfv7qdo6b4ech6zv33cs7xuy6slotxmesglrxnyyxty2jcbykgo37w46tznm2mtejmzedoqjviquqckalxlbcb5fcl5cymoavekeg7fxfvzeewikmgxdtibwv2tiqrqjh2nnz3jnvowwq3nikkdvgtfecclzr2s7zblaibqcaab";
#define A_SEC "gcbajjicaeaafaqbaeaniffigwkdaeluuarlqae2mecbu73lcuis6sg2iwrhlegzipv5mz7sv6jqeuweu54it36xvgw5wrtrhtxvfbamk4lyxuat3vp5olvk24g34v4l3ea4etqcyliizkidceehsciuczxeu2vwt3zrzxyyvbhvz27z4aozjfn7hjpwaswxfqj3zcs4nymdyjqazoroef2bs2553vt57334mh5mqwyh6w72ofzdob6pmrnvh5jfosayeis3s4wll7ag54dyiep5tlxwff7pjr5ew5hoyjemxdo3rrphrusedqum5x7nz5hs2zuzgiszsig5atkrbjaeuaxowced2kex2fqy4bkiuin6lollsijmquynohgqdnlvgrbdaspu23tws3k5nnbw2quuhkngkieextdvf7scwaqdaeaacaucaeabjwvj2eeeiuyyno6v4ppdtl6jrji2gyvhfqw22zuwxclldewfzg2aylv2vq6rocl2vqfeqkn5upyaalsotg7qo2z6wb2h6iogvvlhtexkohugwlrggd7bmjfa3ebfafrbvz3iufblcg2irkea6y5z7sszhvupdcmzvmyvsminsmbczemrxo3difpnqce56vudnc7a4lo6lkrb6a5thgfikjlky3mu6kbotxsdmum4nvjzdeltdyfxeuzu4yhj25lukn7qckunzvngibs3h3efcxclu4fsrzuz3dxkygas73d3gn6mirz4mo3h7kfnza7yhn7kumqq5hhdsjvpd2ulcoz3n5cjrxkabylnbv3k4ve3x3k3mastg52ska2ux5sccvfmhvi672jk3eqnzc64actrcaubqeap7jd3d5ddvho3cujevdyxcdkxafwnwiyicqyarhdpjhdh52uiuczrvsbcqlm2xyrbktshcfmrfiq6etqy6dtouhxav7z4i5yhncklfsglmammlzlc7shk22vjkscbvhqtvujmueyhekv7ik3ykxfeaimrlke2y5fzrwxk6b2yuutekgmyaiv4vye6cjr5owv4jpmuxvteewidakaycagumckmnmrhsj72vquykibby47svcnw6viltdnn7x24lioeqewc6t3lvuauacuj5nwsovcsxxizsp7zrwcwsswbmbvarn3m2lclz6f47wo5ijn32l5ahjtojxrre6uwfexdub3vh36fbflhg3uds4qs4qhcuasrykskj5t46g3a4oah7fw5ycl3rjszhzcxbwgcyxrwavyi3o4qfambacyrvz35pqxqds4uu2hgnyli6usws5xj3l3met3oeqnlj7f766owgmppxrqe637ea5bnc3ze2kn3sgv7dripva5eywahfyhu6ey73xojvbldg3tgwc4pylonxbzcde7uhauiex5t6altqdbxcfrwaeptvyrtmiv4vvp6cutaojo7rrfnj525sgxujo66foztpfrzzh46gnuqlnycqgaqbpu4ibvwkethix3drwifegt67whgym5nkfnhq4ve6csqix45t7him5mzxt4yc67pwt2axfuhr5fqarbwmr5paakhjnr26xiypzbsz3csuqepqjhpmi6uy3wlwd23bwd46vsfpnj7m2w5zko2phvlkg4hlo7ra7ozhb2gmvqvrlkjallybtc6ilhbgxy5vcwxhhv4quvl44wq4ebidaia6zsfplpz2kzlucmpdxtue275yy5hxqxpbmn6tsl3tcvtkncwl277kawntsajgbvrhnbuxwgux4aaaqsnxply4heyg7wpdhwnglbp3fgjcflvrkk6fvj4ysh7ggzj4wl2tzu67aff6zusfp3ke4qx6gpgrz43cmttfkstciuzqunehxk5vwot4gzcitkv7lgo7up6iobiue3ro";
#define B_KEY "gcbaccqcqiaqcafzory5aiortf6w75zjxozacbpqnimqt572tquj6hyaodpyn3cn3srrsp6gk3t6gwoeoslq5ogthrdau7wlkz2obsvp7g7a2o2batlveruiluu2ryqbioufqew4yxgl6r5ln7uttvpvhmlbf76tqqx6ztbm2gdvmlpagen4p44bwmavdeieangrjbxe7a66ckfsap2kqjg2fcygzmywgzati2whz76jhwlx3sruyjekykcwe5o2254durgxtboongkvgbjzxirivx3vrkl5dxbzi3xwpwli7samtqvcwj67xpccqzs35uepc4xbdnkqfkg2rh2m7pglrgmds5zhyn7cyygluaemvmffspbrogbotyvnphj625l6pqhmojmlfiv2seq7zwghbgniul37jcq4mfsnaibqcaab";
#define B_SEC "gcbajiycaeaafaqbaeals5dr2aq5dgl5n73sto5saec7a2qzbh37vhbit4pqa4g7q3we3xfdde74mvxh4nm4i5exb24ngpcgbj7mwvtu4dfk76n6bu5ucbgxkjdiqxjjvdracq5ilajnzromx5d2w37jhhk7koywcl75hbbp5tgczumhkyw6ami3y7zydmybkgiqia2ncsdoj6b54eulea7uvasnukfqnszrmnsbgrvmpt74spmxpxfdjqsivqufmj25vv3yhjcnpgc442mvkmcttorcrlpxlcux2hodsrxpm7mwr7eazhbkfmt57o6efbtfx3ii6fzocg2vakunvcpuz66mxcmyhf3spq36frqmxiaizkykle6dc4mc5hrk26ot5v2x47aoy4sywkrlvejb7tmmocm2rixx6sfbyyle2aqdaeaacaucaeabcwoleiuv7vsh5wlunqgiiuptzidyri63rjjorhajsrtwfnc6k7h4q5vlpeb6527odspuvygg3oximst7wnzsauwac3oqwgstoduqmsrlhnc7u3wmukd52if2loy5zzizfqimev6jndt2ec463hfc5u4soasnokvskhadddqhpzd27wnb6ml534vulnor7bcft32tz36w7knzxmboeh2rcinfkemmxqrojwca5e52twaibh46u3l2i6kbmlqdchxsy5ywouw722kmtkh2ywr5swsul22sdrs74qlffjtfr6h35lqpgvirjvhpex6tmhygjl6ozmdx3mhyz5n4osq2elmat5p6xdmhktn7rsrayxagtkehkelga6a3ldnoupj4wksr2rly6ppvy5phwvl576wagaubqeam3lcwej6zuhgzimcza4rzwh2yruxk44etks6oqnatxkpawulovbn2z3mqfisk7xm4cftnr7wdub3a4euhgcjp2ssucd3ccs6egbi6uxlrevw7az2z5owhhxfq2zklwnnsruosaetn42bxgzxvvkkcaxlen7xpiekmkqpoyplj55is3ali7tr5c3yzokvfyathzsoxsusxcrwpakaycahg2wnggvnhhmedmqkidgs7i7wqk6one7qtm7eczblrra73qj4horbscp2mtcqz3puhrumarauuztm2tcptsl4z2d5erf2iiemgq2j45pc54yhlgndsxwbvixr7nlzepev7csjxhpzj7fgdhifvqrjqq44hq67fhqm3chpbivqdiumbx6hxjex2wh2fwp7byqm7t4emb6t2darqfamapzemfwukusc7eihe3g6esefl4jdr6ve4ulgnsec6rvz5cnk7ztac7y5r7ooavt5tksf2bwroagzwmggz5veenrq2ohedaqhlc55z2rowxc574vgjaoouxghvqrkb4fysoig6gfbr7f62h2iyszrdnv56vh4p5dwhmilqtm6vupshnhmxxtklktorfeyfm5mvfqvafzhbbcokcaubqax5nkgolfn7kapyvsbnfppp6oyb3wbej54jue4ec2f3nxxnxvjxojwkm4q6vj7d24xki4imujzvaght47qrbxvgjqwz6ryen7kv6yuwp63t4ddb3ndmio6jnjln4rjdzqt4tdo5pefkxxn47vnwb5ymknkq2lwgsdrsaswucalvczdtw6prqdog7mhcsbkkxhlorueztxhvxxicqgaqbc5dwzlbxjcpd2twekkhhkne57sk4hd7xacfpn4sslapttjxxgk3x67duz7wwii56ztwmhw4rpgbq2rbhz5jpmxsxsf4elfevjzldijnpf3kfb674aewcchwg2e5jn3tlxbfqt62ztrm2obc5zyb5sbemvv4h25yrxuxunfbc5j3arknnygivkdqshh54i6i5wfuiwibx7vz3i";

int main(int argc, char **argv)
{
  lob_t opts = lob_new();
  fail_unless(e3x_init(opts) == 0);
  fail_unless(!e3x_err());

  e3x_cipher_t cs = e3x_cipher_set(0x2a,NULL);
  if(!cs) return 0;

  cs = e3x_cipher_set(0,"2a");
  fail_unless(cs);
  fail_unless(cs->id == CS_2a);
  
  uint8_t buf[32];
  fail_unless(e3x_rand(buf,32));

  char hex[65];
  util_hex(e3x_hash((uint8_t*)"foo",3,buf),32,hex);
  fail_unless(strcmp(hex,"2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae") == 0);

  lob_t secrets = e3x_generate();
  fail_unless(secrets);
  fail_unless(lob_get(secrets,"2a"));
  lob_t keys = lob_linked(secrets);
  fail_unless(keys);
  fail_unless(lob_get(keys,"2a"));
  LOG("generated key %s secret %s",lob_get(keys,"2a"),lob_get(secrets,"2a"));

  local_t localA = cs->local_new(keys,secrets);
  fail_unless(localA);

  remote_t remoteA = cs->remote_new(lob_get_base32(keys,"2a"), NULL);
  fail_unless(remoteA);

  // create another to start testing real packets
  lob_t secretsB = e3x_generate();
  fail_unless(lob_linked(secretsB));
  local_t localB = cs->local_new(lob_linked(secretsB),secretsB);
  fail_unless(localB);
  remote_t remoteB = cs->remote_new(lob_get_base32(lob_linked(secretsB),"2a"), NULL);
  fail_unless(remoteB);

  // generate a message
  lob_t messageAB = lob_new();
  lob_set_int(messageAB,"a",42);
  lob_t outerAB = cs->remote_encrypt(remoteB,localA,messageAB);
  fail_unless(outerAB);
  fail_unless(lob_len(outerAB) == 553);

  // decrypt and verify it
  lob_t innerAB = cs->local_decrypt(localB,outerAB);
  fail_unless(innerAB);
  fail_unless(lob_get_int(innerAB,"a") == 42);
  fail_unless(cs->remote_verify(remoteA,localB,outerAB) == 0);

  ephemeral_t ephemBA = cs->ephemeral_new(remoteA,outerAB);
  fail_unless(ephemBA);
  
  lob_t channelBA = lob_new();
  lob_set(channelBA,"type","foo");
  lob_t couterBA = cs->ephemeral_encrypt(ephemBA,channelBA);
  fail_unless(couterBA);
  printf("couterBA len %lu\n",lob_len(couterBA));
  fail_unless(lob_len(couterBA) == 62);

  lob_t outerBA = cs->remote_encrypt(remoteA,localB,messageAB);
  fail_unless(outerBA);
  lob_t innerBA = cs->local_decrypt(localA,outerBA);
  fail_unless(innerBA);
  fail_unless(cs->remote_verify(remoteB,localA,outerBA) == 0);
  ephemeral_t ephemAB = cs->ephemeral_new(remoteB,outerBA);
  fail_unless(ephemAB);

  lob_t cinnerAB = cs->ephemeral_decrypt(ephemAB,couterBA);
  fail_unless(cinnerAB);
  fail_unless(util_cmp(lob_get(cinnerAB,"type"),"foo") == 0);

  return 0;
}

