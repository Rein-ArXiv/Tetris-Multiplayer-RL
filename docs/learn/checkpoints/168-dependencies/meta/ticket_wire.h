#pragma once
#include "meta/admission_tickets.h"
#include "meta/account_crypto.h"
#include <string_view>
namespace study_meta {
inline constexpr std::string_view kTicketPrefix="study-join-v1.";
// Same random source boundary as account creation; never use the game RNG.
inline std::optional<Ticket> new_ticket(RandomBytes random=RAND_bytes) {
    Ticket ticket{};
    if(!random || random(ticket.data(),static_cast<int>(ticket.size()))!=1)return std::nullopt;
    return ticket;
}
inline std::string ticket_text(const Ticket& ticket) {
    return std::string(kTicketPrefix)+hex_bytes(ticket.data(),ticket.size());
}
inline std::optional<Ticket> parse_ticket(const std::string& text) {
    Ticket ticket{};
    if(text.size()!=kTicketPrefix.size()+ticket.size()*2 ||
       text.compare(0,kTicketPrefix.size(),kTicketPrefix.data(),kTicketPrefix.size())!=0)return std::nullopt;
    const auto hex=text.substr(kTicketPrefix.size());
    if(!credential_hex(hex,ticket.size()*2))return std::nullopt;
    auto nibble=[](char c)->unsigned char{return static_cast<unsigned char>(c<='9'?c-'0':c-'a'+10);};
    for(std::size_t i=0;i<ticket.size();++i)
        ticket[i]=static_cast<unsigned char>((nibble(hex[i*2])<<4)|nibble(hex[i*2+1]));
    return ticket;
}
} // namespace study_meta
