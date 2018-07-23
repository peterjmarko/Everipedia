// # 2018 Travis Moore, Kedar Iyer, Sam Kazemian
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMmdhhydNMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMNdy    hMMMMMMNdhhmMMMddMMMMMMMMMMMMM
// # MMMMMMMMMMMmh      hMMMMMMh     yMMM  hNMMMMMMMMMM
// # MMMMMMMMMNy       yMMMMMMh       MMMh   hNMMMMMMMM
// # MMMMMMMMd         dMMMMMM       hMMMh     NMMMMMMM
// # MMMMMMMd          dMMMMMN      hMMMm       mMMMMMM
// # MMMMMMm           yMMMMMM      hmmh         NMMMMM
// # MMMMMMy            hMMMMMm                  hMMMMM
// # MMMMMN             hNMMMMMmy                 MMMMM
// # MMMMMm          ymMMMMMMMMmd                 MMMMM
// # MMMMMm         dMMMMMMMMd                    MMMMM
// # MMMMMMy       mMMMMMMMm                     hMMMMM
// # MMMMMMm      dMMMMMMMm                      NMMMMM
// # MMMMMMMd     NMMMMMMM                      mMMMMMM
// # MMMMMMMMd    NMMMMMMN                     mMMMMMMM
// # MMMMMMMMMNy  mMMMMMMM                   hNMMMMMMMM
// # MMMMMMMMMMMmyyNMMMMMMm         hmh    hNMMMMMMMMMM
// # MMMMMMMMMMMMMNmNMMMMMMMNmdddmNNd   ydNMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMNdhyhdmMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMNNMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM

#include <eosiolib/asset.hpp>
#include "eparticlectr.hpp"
#include <typeinfo>

// This function is used to swap endianess for uint64_t's for key and javascript 58-bit int limit issues.
// Once secondary index queries are working, this will be removed
uint64_t eparticlectr::swapEndian64(uint64_t X) {
    uint64_t x = (uint64_t) X;
    x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
    x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
    x = (x & 0x00FF00FF00FF00FF) << 8  | (x & 0xFF00FF00FF00FF00) >> 8;
    return x;
}


// Stake IQ in exchange for brainpower
// Note that the "amount" parameter is in full precision. Dividing it by IQ_PRECISION_MULTIPLIER would give the "clean" amount with a decimal.
void eparticlectr::brainmeart( account_name staker, uint64_t amount ) {
    // The token contract has eosio.code permission for the article contract
    require_auth(ARTICLE_CONTRACT_ACCTNAME);

    uint64_t newBrainpower = amount * IQ_TO_BRAINPOWER_RATIO;

    // Get the brainpower
    brainpwrtbl braintable(ARTICLE_CONTRACT_ACCTNAME, ARTICLE_CONTRACT_ACCTNAME);
    auto brainidx = braintable.get_index<N(byuser)>();
    auto brain_it = brainidx.find(staker);

    // Add the brainpower, creating a new table entry if the staker has never staked before
    if(brain_it == brainidx.end()){
      braintable.emplace( ARTICLE_CONTRACT_ACCTNAME, [&]( auto& b ) {
          b.user = staker;
          b.user_64t = eparticlectr::swapEndian64(staker);
          b.power = newBrainpower;
      });
    }
    else {
      brainidx.modify( brain_it, 0, [&]( auto& b ) {
          b.add(newBrainpower);
      });
    }

    // Create the stake object
    staketbl staketblobj(ARTICLE_CONTRACT_ACCTNAME, ARTICLE_CONTRACT_ACCTNAME);
    staketblobj.emplace( ARTICLE_CONTRACT_ACCTNAME, [&]( auto& s ) {
        s.id = staketblobj.available_primary_key();
        s.user = staker;
        s.user_64t = eparticlectr::swapEndian64(staker);
        s.amount = amount;
        s.timestamp = now();
        s.completion_time = now() + STAKING_DURATION;
    });
}

// Place a vote using the IPFS hash
void eparticlectr::votebyhash ( account_name voter, ipfshash_t& proposed_article_hash, bool approve, uint64_t amount ) {
    require_auth(voter);

    // Check if article exists
    propstbl proptable( _self, _self );
    auto prop_idx = proptable.get_index<N(byhash)>();
    auto prop_it = prop_idx.find(eparticlectr::ipfs_to_key256(proposed_article_hash));
    eosio_assert( prop_it != prop_idx.end(), "proposal not found" );
    uint64_t proposal_id = prop_it->id;

    bool voterIsProposer = (voter == prop_it->proposer);

    // Verify voting is still in progress
    eosio_assert( now() < prop_it->endtime, "voting period is over");

    // Get the brainpower
    brainpwrtbl braintable(_self, _self);
    auto brainidx = braintable.get_index<N(byuser)>();
    auto brain_it = brainidx.find(voter);

    // Consume brainpower
    eosio_assert(brain_it->power >= amount, "Not enough brainpower");
    brainidx.modify( brain_it, 0, [&]( auto& b ) {
        b.sub(amount);
    });

    // Search for existing vote by voter
    votestbl votetbl( _self, _self );
    auto hashidx = votetbl.get_index<N(byhash)>();
    auto voteridx = votetbl.get_index<N(byvoter)>();

    auto hash_it = hashidx.find(eparticlectr::ipfs_to_key256(proposed_article_hash));
    auto voter_it = voteridx.find(voter);

    if (hash_it == hashidx.end() || voter_it == voteridx.end()) {
        // First vote for proposal
        print("FIRST VOTE FOR PROPOSAL BY USER", "\n");
        votetbl.emplace( voter, [&]( auto& a ) {
             a.id = votetbl.available_primary_key();
             a.proposal_id = proposal_id;
             a.proposed_article_hash = proposed_article_hash;
             a.approve = approve;
             a.is_editor = voterIsProposer;
             a.amount = amount;
             a.voter = voter;
             a.voter_64t = eparticlectr::swapEndian64(voter);
             a.timestamp = now();
        });
    }
    else if (voter_it->approve == approve) {
        // Strengthen existing vote
        print("STRENGTHEN EXISTING VOTE", "\n");
        voteridx.modify( voter_it, 0, [&]( auto& a ) {
    	    a.amount += amount;
    	    a.timestamp = now();
        });
    }
    else if (voter_it->amount >= amount) {
	// Weakening existing vote
	print("WEAKEN EXISTING VOTE", "\n");
	voteridx.modify( voter_it, 0, [&]( auto& a ) {
	    a.amount = voter_it->amount - amount;
	    a.timestamp = now();
	});
    }
    else {
	// Switch votes
	print("SWITCH VOTE", "\n");
	voteridx.modify( voter_it, 0, [&]( auto& a ) {
	    a.amount = amount - voter_it->amount;
	    a.approve = approve;
	    a.timestamp = now();
	});
    }
}

void eparticlectr::updatewiki( ipfshash_t& current_hash ){
    // Manually update the wikistbl. This will be removed later.
    require_auth(ARTICLE_CONTRACT_ACCTNAME);

    print("ADDING ARTICLE TO DATABASE\n");
    wikistbl wikitbl( _self, _self );
    auto wikiidx = wikitbl.get_index<N(byhash)>();
    auto wiki_it = wikiidx.find(eparticlectr::ipfs_to_key256(current_hash));
    eosio_assert(wiki_it == wikiidx.end(), "wiki already exists in database");

    wikitbl.emplace( _self,  [&]( auto& a ) {
        a.id = wikitbl.available_primary_key();
        a.hash = current_hash;
        a.parent_hash = "";
    });
}

// Propose an edit for an article
void eparticlectr::propose( account_name proposer, ipfshash_t& proposed_article_hash, ipfshash_t& old_article_hash, ipfshash_t& grandparent_hash ) {
    require_auth(proposer);

    // Temporary check: Only new wikis are allowed
    eosio_assert(old_article_hash == "", "Only new wikis are currently permitted");
    eosio_assert(grandparent_hash == "", "Only new wikis are currently permitted");

    // Fetch the brainpower
    brainpwrtbl braintable(_self, _self);
    auto brainidx = braintable.get_index<N(byuser)>();
    auto brain_it = brainidx.find(proposer);
    eosio_assert( brain_it != brainidx.end(), "No brainpower found");

    // Re-check that enough brainpower is available
    eosio_assert(brain_it->power > EDIT_PROPOSE_BRAINPOWER, "Not enough brainpower to edit, you need to stake some more IQ using everipediaiq::brainmeiq first!");

    // Check for a duplicate proposal
    propstbl proptable( _self, _self );
    auto propidx = proptable.get_index<N(byhash)>();
    auto prop_it = propidx.find(eparticlectr::ipfs_to_key256(proposed_article_hash));
    eosio_assert(prop_it == propidx.end(), "Proposal already exists");

    // Store the proposal
    uint64_t propPrimaryKey = ipfs_to_uint64_trunc(proposed_article_hash);
    proptable.emplace( _self, [&]( auto& a ) {
        a.id = propPrimaryKey; // TODO: Change this once client-side secondary index querying is available
        a.proposed_article_hash = proposed_article_hash;
        a.old_article_hash = old_article_hash;
        a.grandparent_hash = grandparent_hash;
        a.proposer = proposer;
        a.proposer_64t = eparticlectr::swapEndian64(proposer);
        a.starttime = now();
        a.endtime = now() + DEFAULT_VOTING_TIME;
        a.status = ProposalStatus::pending;
    });

    // NO VOTES ADDED FOR NOW. WILL BE ADDED LATER
}

EOSIO_ABI( eparticlectr, (brainmeart)(propose)(updatewiki)(votebyhash) )
