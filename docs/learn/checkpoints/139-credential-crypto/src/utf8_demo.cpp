#include "text/utf8.h"
#include "client/labels.h"
#include <cstdio>
static void inspect(std::string_view input){
    std::size_t offset=0;
    while(!input.empty()){
        const auto item=study_utf8::decode_first(input);
        std::printf("byte %zu: U+%04lX, consumed=%zu, %s\n",offset,
          static_cast<unsigned long>(item.codepoint),item.bytes,
          item.status==study_utf8::Status::scalar?"scalar":"invalid");
        input.remove_prefix(item.bytes);offset+=item.bytes;
    }
}
int main(){
    inspect(u8"A가🙂");
    const char broken[]={char(0xE2),char(0x82),'A'};inspect({broken,sizeof(broken)});
    const char with_zero[]={'A',0,'B'};inspect({with_zero,sizeof(with_zero)});
    const auto labels=study_labels::make();if(!labels)return 1;
    std::printf("menu: %zu bytes, %zu scalars; play: %zu bytes, %zu scalars\n",
      study_labels::menu_text.size(),labels->menu.count,study_labels::play_text.size(),labels->play.count);
}
