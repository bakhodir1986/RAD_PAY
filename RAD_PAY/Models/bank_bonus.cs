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
    
    public partial class bank_bonus
    {
        public long id { get; set; }
        public long bank_id { get; set; }
        public Nullable<long> min_amount { get; set; }
        public Nullable<int> percent { get; set; }
        public Nullable<System.DateTime> start_date { get; set; }
        public Nullable<System.DateTime> end_date { get; set; }
        public Nullable<int> status { get; set; }
        public Nullable<double> longitude { get; set; }
        public Nullable<double> latitude { get; set; }
        public string description { get; set; }
        public Nullable<long> bonus_amount { get; set; }
    
        public virtual bank bank { get; set; }
    }
}
