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
    
    public partial class user
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2214:DoNotCallOverridableMethodsInConstructors")]
        public user()
        {
            this.bills = new HashSet<bill>();
            this.card_monitoring = new HashSet<card_monitoring>();
            this.card_monitoring_cabinet = new HashSet<card_monitoring_cabinet>();
            this.cards = new HashSet<card>();
            this.favorites = new HashSet<favorite>();
            this.news = new HashSet<news>();
            this.notifications = new HashSet<notification>();
            this.periodic_bill = new HashSet<periodic_bill>();
            this.purchases = new HashSet<purchase>();
            this.purchases_reverse = new HashSet<purchases_reverse>();
            this.temp_transaction = new HashSet<temp_transaction>();
            this.temp_transaction1 = new HashSet<temp_transaction>();
            this.top_up_transactions = new HashSet<top_up_transactions>();
            this.transactions = new HashSet<transaction>();
            this.transactions1 = new HashSet<transaction>();
            this.user_bills = new HashSet<user_bills>();
            this.user_bills1 = new HashSet<user_bills>();
            this.user_bonus = new HashSet<user_bonus>();
            this.user_devices = new HashSet<user_devices>();
            this.users_cabinet = new HashSet<users_cabinet>();
            this.users_full_registers = new HashSet<users_full_registers>();
            this.users_online = new HashSet<users_online>();
        }
    
        public long id { get; set; }
        public string phone { get; set; }
        public string password { get; set; }
        public string name { get; set; }
        public System.DateTime registration { get; set; }
        public int sex { get; set; }
        public string notify_token { get; set; }
        public string qrstring { get; set; }
        public Nullable<int> tr_limit { get; set; }
        public string avatar { get; set; }
        public string email { get; set; }
        public string token { get; set; }
        public Nullable<int> lang { get; set; }
        public int blocked { get; set; }
        public Nullable<System.DateTime> purch_ts { get; set; }
        public bool sms_allow { get; set; }
    
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<bill> bills { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<card_monitoring> card_monitoring { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<card_monitoring_cabinet> card_monitoring_cabinet { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<card> cards { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<favorite> favorites { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<news> news { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<notification> notifications { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<periodic_bill> periodic_bill { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<purchase> purchases { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<purchases_reverse> purchases_reverse { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<temp_transaction> temp_transaction { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<temp_transaction> temp_transaction1 { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<top_up_transactions> top_up_transactions { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<transaction> transactions { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<transaction> transactions1 { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<user_bills> user_bills { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<user_bills> user_bills1 { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<user_bonus> user_bonus { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<user_devices> user_devices { get; set; }
        public virtual qr_image qr_image { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<users_cabinet> users_cabinet { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<users_full_registers> users_full_registers { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<users_online> users_online { get; set; }
    }
}