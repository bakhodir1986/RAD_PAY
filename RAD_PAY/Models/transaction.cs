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
    
    public partial class transaction
    {
        public long id { get; set; }
        public Nullable<long> uid { get; set; }
        public string srccard { get; set; }
        public string dstcard { get; set; }
        public string srcphone { get; set; }
        public string dstphone { get; set; }
        public Nullable<long> amount { get; set; }
        public Nullable<System.DateTime> ts { get; set; }
        public Nullable<int> status { get; set; }
        public Nullable<long> dst_uid { get; set; }
        public Nullable<long> commision { get; set; }
        public string ref_num { get; set; }
        public Nullable<long> bearn { get; set; }
        public string status_text { get; set; }
    
        public virtual user user { get; set; }
        public virtual user user1 { get; set; }
    }
}