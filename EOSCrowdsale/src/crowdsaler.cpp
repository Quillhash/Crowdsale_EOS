#include "crowdsaler.hpp"

#include "config.h"

#define NOW now() // rename system func that returns time in seconds

// constructor
crowdsaler::crowdsaler(eosio::name self, eosio::name code, eosio::datastream<const char *> ds) : eosio::contract(self, code, ds),
                                                                                                       state_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                                                       reserved_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                                                       deposits(this->_self, this->_self.value),
                                                                                                       state(state_singleton.exists() ? state_singleton.get() : default_parameters()),
                                                                                                       reserved(reserved_singleton.exists() ? reserved_singleton.get() : default_parameters1())
                                                                                                       
{
}

// destructor
crowdsaler::~crowdsaler()
{
    this->state_singleton.set(this->state, this->_self); // persist the state of the crowdsale before destroying instance

    eosio::print("Saving state to the RAM ");
    eosio::print(this->state.toString());

    this->reserved_singleton.set(this->reserved, this->_self); // persist the state of the crowdsale before destroying instance

    eosio::print("Saving state to the RAM ");
    eosio::print(this->reserved.toString());    
}

// initialize the crowdfund
void crowdsaler::init(eosio::name recipient, eosio::time_point_sec start, eosio::time_point_sec finish)
{
    eosio_assert(!this->state_singleton.exists(), "Already Initialzed");
    eosio_assert(start < finish, "Start must be less than finish");
    require_auth(this->_self);
    eosio_assert(recipient != this->_self,"Recipient should be different than contract deployer");
    // update state
    this->state.recipient = recipient;
    this->state.start = start;
    this->state.finish = finish;
    this->state.pause = false;
}

// issuance of only reserved ZPT
void crowdsaler::issue(eosio::name to, eosio::asset quantity, uint64_t _class, std::string memo)
{
    require_auth(this->state.recipient);
    eosio_assert( is_account( to ), "to account does not exist" );
    eosio_assert( quantity.symbol == sy_zpt ,"Can issue only ZPT coins");
    switch(_class){
        case 1:
        eosio_assert((this->reserved.class1.amount + quantity.amount) <= CLASS1MAX, "Cannot issue more than reserved quantity");
        this->reserved.class1 += quantity;
        case 2:
        eosio_assert((this->reserved.class2.amount + quantity.amount) <= CLASS2MAX, "Cannot issue more than reserved quantity");
        this->reserved.class2 += quantity;
        case 3:
        eosio_assert((this->reserved.class3.amount + quantity.amount) <= CLASS3MAX, "Cannot issue more than reserved quantity");
        this->reserved.class3 += quantity;
        case 4:
        eosio_assert((this->reserved.class4.amount + quantity.amount) <= CLASS4MAX, "Cannot issue more than reserved quantity");
        this->reserved.class4 += quantity;
        case 5:
        eosio_assert((this->reserved.class5.amount + quantity.amount) <= CLASS5MAX, "Cannot issue more than reserved quantity");
        this->reserved.class5 += quantity;
        case 6:
        eosio_assert((this->reserved.class6.amount + quantity.amount) <= CLASS6MAX, "Cannot issue more than reserved quantity");
        this->reserved.class6 += quantity;
        }

    handle_investment(to, quantity.amount);
}

// update contract balances and send ZPT tokens to the investor
void crowdsaler::handle_investment(eosio::name investor, uint64_t tokens_to_give)
{
    // hold the reference to the investor stored in the RAM
    auto it = this->deposits.find(investor.value);

    // if the depositor account was found, store his updated balances
    eosio::asset entire_tokens = eosio::asset(tokens_to_give, eosio::symbol("ZPT", 4));
   
        if (it != this->deposits.end())
        {
            entire_tokens += it->tokens;
        }

        // if the depositor was not found create a new entry in the database, else update his balance
        if (it == this->deposits.end())
        {
            this->deposits.emplace(this->_self, [investor, entire_tokens](auto &deposit) {
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }
        else
        {
            this->deposits.modify(it, this->_self, [investor, entire_tokens](auto &deposit) {
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }

    // set the amounts to transfer, then call inline issue action to update balances in the token contract
    eosio::asset amount = eosio::asset(tokens_to_give, eosio::symbol("ZPT", 4));

    this->inline_issue(investor, amount, "Crowdsale deposit");
    
}

// handle transfers to this contract
void crowdsaler::buyzepta(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo)
{
    eosio_assert(this->state.pause==false, "Crowdsale has been paused" );
    
    // check timings of the eos crowdsale
    eosio_assert(NOW >= this->state.start.utc_seconds, "Crowdsale hasn't started");
    eosio_assert(NOW <= this->state.finish.utc_seconds, "Crowdsale finished");

    // check the minimum and maximum contribution
    eosio_assert(quantity.amount >= MIN_CONTRIB, "Contribution too low");
    eosio_assert((quantity.amount <= MAX_CONTRIB) , "Contribution too high");

    // check if the hard cap was reached
    eosio_assert(this->state.total_tokens.amount <= GOAL, "GOAL reached");
    
    //calculate 3% fees on buying ZPT
    eosio::asset fees ;
    fees.amount = quantity.amount*0.03;
    fees.symbol = quantity.symbol;
    

    if(to == eosio::name("crowdsaler")){  
        this->state.total_eoses.amount += quantity.amount;
    
        quantity-=fees;
        // dont send fees to _self
        // else ZPT supply would increase

         // calculate from EOS to tokens
        int64_t tokens_to_give = (quantity.amount)/RATE;

        this->state.total_tokens.amount += tokens_to_give; // calculate from EOS to tokens

    // handle investment
        this->handle_investment(from, tokens_to_give);
    }
}


// toggles unpause / pause contract
void crowdsaler::pause()
{
    require_auth(this->state.recipient);
    if (state.pause==false)
        this->state.pause = true; 
    else
        this->state.pause = false;
}


void crowdsaler::rate()
{
    eosio::print("Rate of 1 ZPT token:");
    eosio::print(RATE);
}

void crowdsaler::checkgoal()
{
    if(this->state.total_tokens.amount <= GOAL)
    {
        eosio::print("Goal not Reached:");
        eosio::print(GOAL);
    }
    else
        eosio::print("Goal Reached");
}

// used for trading.
void crowdsaler::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo)
{
    require_auth(from);

    // calculate fees and send by rate.......
    this->inline_transfer(from, to, quantity, "trading ");
  
}

// used by recipient to withdraw EOS
void crowdsaler::withdraw()
{
    require_auth(this->state.recipient);
 // NOTE: UNCOMMENT THESE
 //  eosio_assert(NOW >= this->state.finish.utc_seconds, "Crowdsale not ended yet" );
 //  eosio_assert(this->state.total_eoses.amount >= SOFT_CAP_TKN, "Soft cap was not reached");
    eosio::asset all_eoses;
    all_eoses = this->state.total_eoses;
    this->inline_transfer(this->_self, this->state.recipient, all_eoses, "withdraw by recipient");
    //  crowdsaler deposit of eos deleted from table (which equals to total eoses in state)
    this->state.total_eoses.amount = 0;
    //auto it = this->deposits.find(("crowdsaler"_n).value);
    //this->deposits.erase(it);
}


// custom dispatcher that handles ZPT transfers from quilltoken contract
extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
    if (code == eosio::name("quilltoken").value && action == eosio::name("transfer").value) // handle actions from eosio.token contract
    {
        eosio::execute_action(eosio::name(receiver), eosio::name(code), &crowdsaler::buyzepta);
    }
    else if (code == receiver) // for other direct actions
    {
        switch (action)
        {
            EOSIO_DISPATCH_HELPER(crowdsaler, (init)(transfer)(buyzepta)(issue)(pause)(rate)(checkgoal)(withdraw));
        }
    }
}