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
    
    public partial class merchant_api
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2214:DoNotCallOverridableMethodsInConstructors")]
        public merchant_api()
        {
            this.merchants = new HashSet<merchant>();
        }
    
        public int id { get; set; }
        public string name { get; set; }
        public string url { get; set; }
        public string login { get; set; }
        public string password { get; set; }
        public string api_json { get; set; }
        public string options { get; set; }
        public Nullable<int> status { get; set; }
        public int api_id { get; set; }
        public Nullable<int> b { get; set; }
    
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<merchant> merchants { get; set; }
    }
}
