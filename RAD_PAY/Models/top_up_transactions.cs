//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated from a template.
//
//     Manual changes to this file may cause unexpected behavior in your application.
//     Manual changes to this file will be overwritten if the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace RAD_PAY.Models
{
    using System;
    using System.Collections.Generic;
    
    public partial class top_up_transactions
    {
        public long id { get; set; }
        public Nullable<int> topup_id { get; set; }
        public Nullable<long> amount_sum { get; set; }
        public Nullable<double> amount_req { get; set; }
        public Nullable<int> currency { get; set; }
        public Nullable<long> uid { get; set; }
        public string login { get; set; }
        public Nullable<System.DateTime> ts { get; set; }
        public Nullable<System.DateTime> tse { get; set; }
        public Nullable<int> status { get; set; }
        public string status_text { get; set; }
        public Nullable<long> card_id { get; set; }
        public string card_pan { get; set; }
        public string eopc_trn_id { get; set; }
        public Nullable<long> rad_card { get; set; }
        public string pay_description { get; set; }
        public Nullable<long> topup_trn_id { get; set; }
    
        public virtual card card { get; set; }
        public virtual top_up_merchants top_up_merchants { get; set; }
        public virtual user user { get; set; }
    }
}
